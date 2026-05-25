# Included from ../../../Makefile; paths are relative to src/.

MONITOR_EXEC_OBJS := app/selftest/monitor/monitor_exec.o
MONITOR_C_OBJS := $(MONITOR_EXEC_OBJS)

MONITOR_EXEC_ELF := app/selftest/monitor/monitor_exec.elf
MONITOR_EXEC_BIN := app/selftest/monitor/monitor_exec.bin
MONITOR_EXEC_BLOB := app/selftest/monitor/monitor_exec_blob.bin

$(MONITOR_EXEC_ELF): $(MONITOR_EXEC_OBJS) app/selftest/monitor/monitor.ld
	$(LD) -m elf_i386 --gc-sections -T app/selftest/monitor/monitor.ld -Map app/selftest/monitor/monitor_exec.map -o $@ $(MONITOR_EXEC_OBJS) $(LIBGCC)

$(MONITOR_EXEC_BIN): $(MONITOR_EXEC_ELF)
	$(OBJCOPY) -O binary $< $@

$(MONITOR_EXEC_BLOB): $(MONITOR_EXEC_BIN) $(BUILD_SCRIPTS)/gen_selftest_exec_blob.py include/selftest_exec_blob.h
	python3 $(BUILD_SCRIPTS)/gen_selftest_exec_blob.py $< 0x00180000 $@
