const std = @import("std");

const c = @cImport({
    @cInclude("errno.h");
    @cInclude("fcntl.h");
    @cInclude("signal.h");
    @cInclude("string.h");
    @cInclude("sys/reboot.h");
    @cInclude("sys/ioctl.h");
    @cInclude("sys/mount.h");
    @cInclude("sys/stat.h");
    @cInclude("sys/types.h");
    @cInclude("sys/wait.h");
    @cInclude("termios.h");
    @cInclude("unistd.h");
});

fn errnoValue() c_int {
    return c.__errno_location().*;
}

fn writeAllFd(fd: c_int, msg: []const u8) void {
    var pos: usize = 0;
    while (pos < msg.len) {
        const n = c.write(fd, msg.ptr + pos, msg.len - pos);
        if (n <= 0) return;
        pos += @intCast(n);
    }
}

fn log(comptime fmt: []const u8, args: anytype) void {
    var buf: [256]u8 = undefined;
    const msg = std.fmt.bufPrint(&buf, fmt, args) catch return;
    writeAllFd(2, msg);
}

fn logErrno(op: []const u8, path: []const u8) void {
    const e = errnoValue();
    const msg = std.mem.span(c.strerror(e));
    log("init: {s} {s}: {s}\n", .{ op, path, msg });
}

fn mutCStr(s: [*:0]const u8) [*c]u8 {
    return @ptrCast(@constCast(s));
}

fn ensureDir(path: [*:0]const u8, mode: c.mode_t) void {
    if (c.mkdir(path, mode) == 0 or errnoValue() == c.EEXIST) return;
    logErrno("mkdir", std.mem.span(path));
}

fn makeDev(major_no: u32, minor_no: u32) c.dev_t {
    const major64: u64 = major_no;
    const minor64: u64 = minor_no;
    const dev =
        (minor64 & 0xff) |
        ((major64 & 0xfff) << 8) |
        ((minor64 & ~@as(u64, 0xff)) << 12) |
        ((major64 & ~@as(u64, 0xfff)) << 32);
    return @intCast(dev);
}

fn ensureChr(path: [*:0]const u8, mode: c.mode_t, major_no: u32, minor_no: u32) void {
    const node_mode: c.mode_t = @as(c.mode_t, @intCast(c.S_IFCHR)) | mode;
    if (c.mknod(path, node_mode, makeDev(major_no, minor_no)) == 0 or
        errnoValue() == c.EEXIST)
    {
        return;
    }
    logErrno("mknod", std.mem.span(path));
}

fn ensureBasicDevNodes() void {
    ensureChr("/dev/console", 0o600, 5, 1);
    ensureChr("/dev/tty", 0o666, 5, 0);
    ensureChr("/dev/ttyS0", 0o600, 4, 64);
    ensureChr("/dev/null", 0o666, 1, 3);
}

fn mountOne(
    source: [*:0]const u8,
    target: [*:0]const u8,
    fs_type: [*:0]const u8,
    flags: c_ulong,
    data: [*:0]const u8,
) void {
    if (c.mount(source, target, fs_type, flags, data) == 0 or errnoValue() == c.EBUSY) {
        return;
    }
    logErrno("mount", std.mem.span(target));
}

fn openConsole() c_int {
    const paths = [_][*:0]const u8{
        "/dev/console",
        "/dev/ttyS0",
        "/dev/tty0",
        "/dev/tty1",
        "/dev/null",
    };

    for (paths) |path| {
        const fd = c.open(path, c.O_RDWR | c.O_NOCTTY);
        if (fd >= 0) return fd;
    }
    return -1;
}

fn setupConsole(controlling_tty: bool) void {
    if (controlling_tty and c.setsid() < 0 and errnoValue() != c.EPERM) {
        logErrno("setsid", "");
    }

    const fd = openConsole();
    if (fd < 0) {
        log("init: no console device\n", .{});
        return;
    }

    if (controlling_tty and
        c.ioctl(fd, c.TIOCSCTTY, @as(c_int, 0)) < 0 and
        errnoValue() != c.EPERM and errnoValue() != c.ENOTTY)
    {
        logErrno("TIOCSCTTY", "/dev/console");
    }

    _ = c.dup2(fd, 0);
    _ = c.dup2(fd, 1);
    _ = c.dup2(fd, 2);
    if (fd > 2) {
        _ = c.close(fd);
    }
}

fn mountBasicFilesystems() void {
    ensureDir("/dev", 0o755);
    mountOne("devtmpfs", "/dev", "devtmpfs", c.MS_NOSUID, "mode=0755");
    ensureBasicDevNodes();
    setupConsole(false);

    ensureDir("/run", 0o755);
    mountOne("tmpfs", "/run", "tmpfs", c.MS_NOSUID | c.MS_NODEV, "mode=0755,size=4m");
    ensureDir("/run/udev", 0o755);

    ensureDir("/proc", 0o555);
    ensureDir("/sys", 0o555);
    mountOne("proc", "/proc", "proc", c.MS_NOSUID | c.MS_NOEXEC | c.MS_NODEV, "");
    mountOne("sysfs", "/sys", "sysfs", c.MS_NOSUID | c.MS_NOEXEC | c.MS_NODEV, "");
}

fn firstExecutable(paths: []const [*:0]const u8) ?[*:0]const u8 {
    for (paths) |path| {
        if (c.access(path, c.X_OK) == 0) {
            return path;
        }
    }
    return null;
}

fn runAndWait(name: []const u8, argv: [*c]const [*c]u8) c_int {
    const pid = c.fork();
    if (pid < 0) {
        logErrno("fork", name);
        return -1;
    }

    if (pid == 0) {
        var envp = [_][*c]u8{
            mutCStr("HOME=/"),
            mutCStr("PATH=/bin:/sbin:/usr/bin:/usr/sbin"),
            mutCStr("TERM=linux"),
            null,
        };

        _ = c.execve(argv[0], argv, @ptrCast(&envp));
        logErrno("exec", name);
        c._exit(127);
    }

    while (true) {
        var status: c_int = 0;
        const got = c.waitpid(pid, &status, 0);
        if (got == pid) {
            if (status != 0) {
                log("init: {s} exited status={x}\n", .{ name, status });
            }
            return status;
        }
        if (got < 0 and errnoValue() == c.EINTR) continue;
        logErrno("waitpid", name);
        return -1;
    }
}

fn startUdevDaemon(path: [*:0]const u8) void {
    const pid = c.fork();
    if (pid < 0) {
        logErrno("fork", std.mem.span(path));
        return;
    }

    if (pid == 0) {
        var argv = [_][*c]u8{ mutCStr(path), mutCStr("--daemon"), null };
        var envp = [_][*c]u8{
            mutCStr("HOME=/"),
            mutCStr("PATH=/bin:/sbin:/usr/bin:/usr/sbin"),
            mutCStr("TERM=linux"),
            null,
        };

        _ = c.execve(path, @ptrCast(&argv), @ptrCast(&envp));
        logErrno("exec", std.mem.span(path));
        c._exit(127);
    }
}

fn runUdevAdm(udevadm: [*:0]const u8, arg1: [*:0]const u8, arg2: ?[*:0]const u8) void {
    if (arg2) |a2| {
        var argv = [_][*c]u8{ mutCStr(udevadm), mutCStr(arg1), mutCStr(a2), null };
        _ = runAndWait(std.mem.span(arg1), @ptrCast(&argv));
    } else {
        var argv = [_][*c]u8{ mutCStr(udevadm), mutCStr(arg1), null };
        _ = runAndWait(std.mem.span(arg1), @ptrCast(&argv));
    }
}

fn startUdev() void {
    const udevd_paths = [_][*:0]const u8{
        "/lib/systemd/systemd-udevd",
        "/usr/lib/systemd/systemd-udevd",
        "/sbin/udevd",
        "/usr/lib/udev/udevd",
    };
    const udevadm_paths = [_][*:0]const u8{
        "/sbin/udevadm",
        "/usr/sbin/udevadm",
        "/bin/udevadm",
        "/usr/bin/udevadm",
    };

    const udevd = firstExecutable(&udevd_paths) orelse {
        log("init: udevd not found\n", .{});
        return;
    };

    log("init: starting {s}\n", .{std.mem.span(udevd)});
    startUdevDaemon(udevd);
    _ = c.usleep(200000);
    reapNonblock();

    const udevadm = firstExecutable(&udevadm_paths) orelse {
        log("init: udevadm not found\n", .{});
        return;
    };

    runUdevAdm(udevadm, "trigger", "--action=add");
    runUdevAdm(udevadm, "settle", "--timeout=10");
    reapNonblock();
}

fn readSmallFile(path: [*:0]const u8, buf: []u8) ?[]const u8 {
    const fd = c.open(path, c.O_RDONLY | c.O_CLOEXEC);
    if (fd < 0) {
        return null;
    }
    defer _ = c.close(fd);

    const n = c.read(fd, buf.ptr, buf.len);
    if (n <= 0) {
        return null;
    }
    return buf[0..@intCast(n)];
}

fn consoleIsTtyS0() bool {
    var buf: [128]u8 = undefined;
    const active = readSmallFile("/sys/class/tty/console/active", &buf) orelse {
        log("init: cannot read console active tty; assuming ttyS0 is free\n", .{});
        return false;
    };

    log("init: active console: {s}", .{active});
    return std.mem.indexOf(u8, active, "ttyS0") != null;
}

fn startPppOnTtyS0() void {
    const pppd_paths = [_][*:0]const u8{
        "/usr/sbin/pppd",
        "/sbin/pppd",
        "/usr/bin/pppd",
        "/bin/pppd",
    };

    if (consoleIsTtyS0()) {
        log("init: ttyS0 is console; ppp disabled\n", .{});
        return;
    }

    const pppd = firstExecutable(&pppd_paths) orelse {
        log("init: pppd not found; ttyS0 ppp disabled\n", .{});
        return;
    };

    const pid = c.fork();
    if (pid < 0) {
        logErrno("fork", std.mem.span(pppd));
        return;
    }

    if (pid == 0) {
        var argv = [_][*c]u8{
            mutCStr(pppd),
            mutCStr("/dev/ttyS0"),
            mutCStr("115200"),
            mutCStr("local"),
            mutCStr("noauth"),
            mutCStr("nodetach"),
            null,
        };
        var envp = [_][*c]u8{
            mutCStr("HOME=/"),
            mutCStr("PATH=/bin:/sbin:/usr/bin:/usr/sbin"),
            mutCStr("TERM=linux"),
            null,
        };

        _ = c.setsid();
        _ = c.execve(pppd, @ptrCast(&argv), @ptrCast(&envp));
        logErrno("exec", std.mem.span(pppd));
        c._exit(127);
    }

    log("init: started pppd on ttyS0 pid={d}\n", .{pid});
}

fn startShell() c.pid_t {
    const pid = c.fork();
    if (pid < 0) {
        logErrno("fork", "/bin/bash");
        return -1;
    }

    if (pid == 0) {
        var argv = [_][*c]u8{ mutCStr("bash"), null };
        var envp = [_][*c]u8{
            mutCStr("HOME=/"),
            mutCStr("PATH=/bin:/sbin:/usr/bin:/usr/sbin"),
            mutCStr("TERM=linux"),
            null,
        };

        setupConsole(true);
        _ = c.execve("/bin/bash", @ptrCast(&argv), @ptrCast(&envp));
        logErrno("exec", "/bin/bash");
        c._exit(127);
    }

    return pid;
}

fn reapNonblock() void {
    while (true) {
        var status: c_int = 0;
        const pid = c.waitpid(-1, &status, c.WNOHANG);
        if (pid > 0) continue;
        if (pid == 0 or errnoValue() == c.ECHILD) return;
        if (errnoValue() == c.EINTR) continue;
        logErrno("waitpid", "");
        return;
    }
}

fn waitForShell(shell_pid: c.pid_t) void {
    while (true) {
        var status: c_int = 0;
        const pid = c.waitpid(-1, &status, 0);
        if (pid == shell_pid) return;
        if (pid > 0) continue;
        if (errnoValue() == c.EINTR) continue;
        if (errnoValue() != c.ECHILD) {
            logErrno("waitpid", "");
        }
        return;
    }
}

fn terminateLeftoverChildren() void {
    _ = c.kill(-1, c.SIGTERM);
    for (0..20) |_| {
        reapNonblock();
        _ = c.usleep(100000);
    }

    _ = c.kill(-1, c.SIGKILL);
    for (0..20) |_| {
        reapNonblock();
        _ = c.usleep(100000);
    }
}

fn unmountOne(path: [*:0]const u8) void {
    if (c.umount2(path, c.MNT_DETACH) == 0 or
        errnoValue() == c.EINVAL or errnoValue() == c.ENOENT)
    {
        return;
    }
    logErrno("umount", std.mem.span(path));
}

fn unmountBasicFilesystems() void {
    c.sync();
    unmountOne("/proc");
    unmountOne("/sys");
    unmountOne("/dev");
    unmountOne("/run");
    _ = c.chdir("/");
    unmountOne("/");
    c.sync();
}

fn shutdownSystem() void {
    log("init: poweroff\n", .{});
    _ = c.reboot(@as(c_int, @bitCast(@as(u32, c.RB_POWER_OFF))));
    logErrno("reboot", "RB_POWER_OFF");

    log("init: halt\n", .{});
    _ = c.reboot(@as(c_int, @bitCast(@as(u32, c.RB_HALT_SYSTEM))));
    logErrno("reboot", "RB_HALT_SYSTEM");
}

pub fn main() void {
    mountBasicFilesystems();
    startUdev();
    log("init: starting /bin/sh\n", .{});

    const shell_pid = startShell();
    if (shell_pid > 0) {
        waitForShell(shell_pid);
    }

    log("init: /bin/sh exited\n", .{});
    terminateLeftoverChildren();
    unmountBasicFilesystems();
    shutdownSystem();

    log("init: halted\n", .{});
    while (true) {
        _ = c.pause();
    }
}
