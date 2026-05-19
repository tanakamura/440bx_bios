CC := gcc
LD := ld
OBJCOPY := objcopy
LIBGCC := $(shell $(CC) -m32 -print-libgcc-file-name)
CPPFLAGS := -I.
CFLAGS := -O2 -m32 -march=pentium2 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fstack-usage -Wstack-usage=256 -nostdlib -nostartfiles -nodefaultlibs -Wall -Wextra
NASMFLAGS := -I.
OBJCOPY_ELF_FLAGS := -I binary -O elf32-i386 -B i386
UACPI_CPPFLAGS := $(CPPFLAGS) -I../uACPI/include -DUACPI_OVERRIDE_CONFIG
UACPI_CFLAGS := $(CFLAGS) -Os -ffunction-sections -fdata-sections \
	-Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers
UACPI_SRC_NAMES := tables types uacpi utilities interpreter opcodes namespace \
	stdlib shareable opregion default_handlers io notify sleep registers \
	resources event mutex osi
UACPI_OBJS := $(addsuffix .o,$(addprefix uacpi_,$(UACPI_SRC_NAMES)))

post_code.h post_code.inc ../specs/POST_CODE.md: post_code.def gen_post_codes.py
	python3 gen_post_codes.py

fdos_blob.bin: ../freedos_boot_fd.img gen_blob.py
	python3 gen_blob.py --fat12-normalize ../freedos_boot_fd.img fdos_blob.bin

fdos_flat_test_blob.bin: custflat.img gen_blob.py
	python3 gen_blob.py --fat12-normalize custflat.img fdos_flat_test_blob.bin

fdos_blob.o: fdos_blob.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.fdos,alloc,load,readonly,data,contents \
		$< $@

fdos_flat_test_blob.o: fdos_flat_test_blob.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.fdos,alloc,load,readonly,data,contents \
		$< $@

dsdt.aml: ../specs/dsdt.dsl gen_dsdt_aml.py
	python3 gen_dsdt_aml.py ../specs/dsdt.dsl dsdt.aml

dsdt_blob.bin: dsdt.aml gen_blob.py
	python3 gen_blob.py dsdt.aml dsdt_blob.bin

dsdt_blob.o: dsdt_blob.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.dsdtblob,alloc,load,readonly,data,contents \
		$< $@

vgabios_blob.bin: ../images/gbsmc.088 gen_blob.py
	python3 gen_blob.py ../images/gbsmc.088 vgabios_blob.bin

vgabios_blob.o: vgabios_blob.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.vgabiosblob,alloc,load,readonly,data,contents \
		$< $@

testfd.img: gen_testfd.py
	python3 gen_testfd.py

int13probe.bin: int13probe.asm
	nasm -f bin $< -o $@

int13probe.img: int13probe.bin gen_int13probe.py
	python3 gen_int13probe.py

usbmbr.bin: usbmbr.asm
	nasm -f bin $< -o $@

usbmbr.img: usbmbr.bin
	cp $< $@
	truncate -s 1474560 $@

linuxprobe.o: linuxprobe.asm
	nasm -f elf32 $< -o $@

linuxprobe.elf: linuxprobe.o linuxprobe.ld
	$(LD) -m elf_i386 -T linuxprobe.ld -o $@ linuxprobe.o

linuxprobe.img linuxprobe_raw.img: linuxprobe.elf gen_linuxprobe_img.py
	python3 gen_linuxprobe_img.py

s3test.o: s3test.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

s3test_uacpi.o: s3test_uacpi.c uacpi_config.h
	$(CC) $(UACPI_CPPFLAGS) $(UACPI_CFLAGS) -c $< -o $@

uacpi_%.o: ../uACPI/source/%.c uacpi_config.h
	$(CC) $(UACPI_CPPFLAGS) $(UACPI_CFLAGS) -c $< -o $@

s3test.elf: s3test.o s3test_uacpi.o $(UACPI_OBJS) s3test.ld
	$(LD) -m elf_i386 --gc-sections -T s3test.ld -Map s3test.map -o $@ s3test.o s3test_uacpi.o $(UACPI_OBJS) $(LIBGCC)

test_elf_blob.bin: s3test.elf gen_blob.py
	python3 gen_blob.py s3test.elf test_elf_blob.bin

test_elf_blob.o: test_elf_blob.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.testelfblob,alloc,load,readonly,data,contents \
		$< $@

srecsave.com: ../tools/dos/srecsave.asm
	nasm -f bin $< -o $@

install-srecsave: srecsave.com
	mcopy -o -i ../freedos_boot_fd.img srecsave.com ::SRECSAVE.COM

xrecv.exe: ../tools/dos/xrecv.c
	WATCOM=/opt/watcom INCLUDE=/opt/watcom/h PATH=/opt/watcom/binl64:$$PATH /opt/watcom/binl64/owcc -b dos -mcmodel=s -os -s $< -o $@

install-xrecv-exe: xrecv.exe
	mcopy -o -i ../freedos_boot_fd.img xrecv.exe ::XRECV.EXE

flashutil_io.obj: ../tools/dos/flashutil_io.asm
	nasm -f obj $< -o $@

flashutil.exe: ../tools/dos/flashutil.c flashutil_io.obj
	WATCOM=/opt/watcom INCLUDE=/opt/watcom/h PATH=/opt/watcom/binl64:$$PATH /opt/watcom/binl64/owcc -b dos -mcmodel=s -os -s ../tools/dos/flashutil.c flashutil_io.obj -o $@

install-flashutil-exe: flashutil.exe
	mcopy -o -i ../freedos_boot_fd.img flashutil.exe ::FLASHUTL.EXE

mem_dmp.exe: ../tools/dos/mem_dmp.c
	WATCOM=/opt/watcom INCLUDE=/opt/watcom/h PATH=/opt/watcom/binl64:$$PATH /opt/watcom/binl64/owcc -b dos -mcmodel=s -os -s $< -o $@

install-mem-dmp-exe: mem_dmp.exe
	mcopy -o -i ../freedos_boot_fd.img mem_dmp.exe ::MEM_DMP.EXE

flashtest.img: flashutil.exe gen_flashtest_img.py
	python3 gen_flashtest_img.py

biostest.exe: ../tools/dos/biostest.c
	WATCOM=/opt/watcom INCLUDE=/opt/watcom/h PATH=/opt/watcom/binl64:$$PATH /opt/watcom/binl64/owcc -b dos -mcmodel=s -os -s $< -o $@

flattest.exe: ../tools/dos/flattest.c
	WATCOM=/opt/watcom INCLUDE=/opt/watcom/h PATH=/opt/watcom/binl64:$$PATH /opt/watcom/binl64/owcc -b dos -mcmodel=s -os -s $< -o $@

shutdown.exe: ../tools/dos/shutdown.c
	WATCOM=/opt/watcom INCLUDE=/opt/watcom/h PATH=/opt/watcom/binl64:$$PATH /opt/watcom/binl64/owcc -b dos -mcmodel=s -os -s $< -o $@

stdtest.img custbio.img custflat.img: biostest.exe flattest.exe shutdown.exe flashutil.exe gen_test_images.py
	python3 gen_test_images.py

test10.img: gen_test10.py
	python3 gen_test10.py

test10_sparse.bin: test10.img gen_fdos_sparse.py
	python3 gen_fdos_sparse.py test10.img test10_sparse.bin

test10_sparse.o: test10_sparse.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.test10,alloc,load,readonly,data,contents \
		$< $@

testfd_sparse.bin: testfd.img gen_fdos_sparse.py
	python3 gen_fdos_sparse.py testfd.img testfd_sparse.bin

testfd_sparse.o: testfd_sparse.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.testfd,alloc,load,readonly,data,contents \
		$< $@

test_floppy_blob.bin: testfd.img gen_fdos_sparse.py
	python3 gen_fdos_sparse.py testfd.img test_floppy_blob.bin

bios.bin: bios.elf
	$(OBJCOPY) -O binary $< $@

bios_blob.bin: bios.bin gen_blob.py
	python3 gen_blob.py bios.bin bios_blob.bin

bios_blob.o: bios_blob.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.biosblob,alloc,load,readonly,data,contents \
		$< $@

stage2.bin: stage2.elf
	$(OBJCOPY) -O binary $< $@

stage2_blob.bin: stage2.bin gen_blob.py
	python3 gen_blob.py stage2.bin stage2_blob.bin

stage2_blob.o: stage2_blob.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.stage2blob,alloc,load,readonly,data,contents \
		$< $@

qemu_stage2.bin: qemu_stage2.elf
	$(OBJCOPY) -O binary $< $@

qemu_stage2_blob.bin: qemu_stage2.bin gen_blob.py
	python3 gen_blob.py qemu_stage2.bin qemu_stage2_blob.bin

qemu_stage2_blob.o: qemu_stage2_blob.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.stage2blob,alloc,load,readonly,data,contents \
		$< $@

start: start.elf
	$(OBJCOPY) -O binary --pad-to 0x100000 --gap-fill 0x90 $< $@

start.elf: start_asm.o bootblock.o blob_service.o stage2_blob.o bios_blob.o dsdt_blob.o vgabios_blob.o test_elf_blob.o p2b98_xv/stage1/start.ld
	$(LD) -m elf_i386 -T p2b98_xv/stage1/start.ld -Map start.map -o $@ start_asm.o bootblock.o blob_service.o stage2_blob.o bios_blob.o dsdt_blob.o vgabios_blob.o test_elf_blob.o

bios.elf: bios16.o stage3_entry.o stage3_runtime.o bios_acpi_runtime.o bios_benchmark.o bios_legacy.o bios_linux.o bios_maintenance.o bios_memtest.o bios_memory.o bios_nvram.o bios_rtc.o bios_selftest.o bios_serial.o bios_settings.o bios_shadow.o bios_stage_context.o bios_pci.o bios_storage.o linux_loader.o legacy_bda.o legacy_boot.o legacy_debug.o legacy_floppy.o legacy_int13.o legacy_int15.o legacy_keyboard.o legacy_misc.o legacy_platform.o legacy_runtime.o legacy_service.o legacy_thunk.o legacy_timer.o legacy_time.o legacy_video.o bios.ld
	$(LD) -m elf_i386 -T bios.ld -Map bios.map -o $@ bios16.o stage3_entry.o stage3_runtime.o bios_acpi_runtime.o bios_benchmark.o bios_legacy.o bios_linux.o bios_maintenance.o bios_memtest.o bios_memory.o bios_nvram.o bios_rtc.o bios_selftest.o bios_serial.o bios_settings.o bios_shadow.o bios_stage_context.o bios_pci.o bios_storage.o linux_loader.o legacy_bda.o legacy_boot.o legacy_debug.o legacy_floppy.o legacy_int13.o legacy_int15.o legacy_keyboard.o legacy_misc.o legacy_platform.o legacy_runtime.o legacy_service.o legacy_thunk.o legacy_timer.o legacy_time.o legacy_video.o

stage2.elf: stage2.o l2_service.o acpi_tables.o p2b98_xv/stage2/stage2.ld
	$(LD) -m elf_i386 -T p2b98_xv/stage2/stage2.ld -Map stage2.map -o $@ stage2.o l2_service.o acpi_tables.o

qemu_bios.bin: qemu_start.elf
	$(OBJCOPY) -O binary $< $@
	truncate -s 262144 $@

qemu_start.elf: qemu_start.o qemu_stage1.o blob_service.o qemu_stage2_blob.o bios_blob.o test_elf_blob.o qemu/stage1/qemu.ld
	$(LD) -m elf_i386 -T qemu/stage1/qemu.ld -Map qemu.map -o $@ qemu_start.o qemu_stage1.o blob_service.o qemu_stage2_blob.o bios_blob.o test_elf_blob.o

qemu_legacy_bios.bin: qemu_legacy_start.elf
	$(OBJCOPY) -O binary $< $@
	truncate -s 262144 $@

qemu_legacy_start.elf: qemu_start.o qemu_stage1.o blob_service.o qemu_stage2_blob.o bios_blob.o qemu/stage1/qemu.ld
	$(LD) -m elf_i386 -T qemu/stage1/qemu.ld -Map qemu_legacy.map -o $@ qemu_start.o qemu_stage1.o blob_service.o qemu_stage2_blob.o bios_blob.o

qemu_flat_test_bios.bin: qemu_flat_test_start.elf
	$(OBJCOPY) -O binary $< $@
	truncate -s 262144 $@

qemu_flat_test_start.elf: qemu_start.o qemu_stage1.o blob_service.o qemu_stage2_blob.o bios_blob.o test_elf_blob.o qemu/stage1/qemu.ld
	$(LD) -m elf_i386 -T qemu/stage1/qemu.ld -Map qemu_flat_test.map -o $@ qemu_start.o qemu_stage1.o blob_service.o qemu_stage2_blob.o bios_blob.o test_elf_blob.o

qemu_legacy_test_bios.bin: qemu_legacy_bios.bin test_floppy_blob.bin patch_rom_free.py
	cp qemu_legacy_bios.bin $@
	python3 patch_rom_free.py $@ test_floppy_blob.bin

start_legacy: start_asm.o bootblock.o blob_service.o stage2_blob.o bios_blob.o p2b98_xv/stage1/start.ld
	$(LD) -m elf_i386 -T p2b98_xv/stage1/start.ld -Map start_legacy.map -o start_legacy.elf start_asm.o bootblock.o blob_service.o stage2_blob.o bios_blob.o
	$(OBJCOPY) -O binary --pad-to 0x100000 --gap-fill 0x90 start_legacy.elf $@

start_legacy_test: start_legacy test_floppy_blob.bin patch_rom_free.py
	cp start_legacy $@
	python3 patch_rom_free.py $@ test_floppy_blob.bin

start_asm.o: p2b98_xv/stage1/start.asm post_code.inc
	nasm $(NASMFLAGS) -f elf32 $< -o $@

bootblock.o: p2b98_xv/stage1/bootblock.c post_code.h blob.h shared_service/service_table.h bios_nvram.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

stage2.o: p2b98_xv/stage2/stage2.c acpi_tables.h blob.h shared_service/service_table.h p2b98_xv/stage2/l2_service.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

qemu_stage2.o: qemu/stage2/stage2.c acpi_tables.h blob.h shared_service/service_table.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

acpi_tables.o: acpi_tables.c acpi_tables.h bios_memory.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

blob_service.o: shared_service/blob_service.c blob.h shared_service/service_table.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

l2_service.o: p2b98_xv/stage2/l2_service.c p2b98_xv/stage2/l2_service.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

qemu_stage2.elf: qemu_stage2.o acpi_tables.o qemu/stage2/stage2.ld
	$(LD) -m elf_i386 -T qemu/stage2/stage2.ld -Map qemu_stage2.map -o $@ qemu_stage2.o acpi_tables.o

bios16.o: app/legacy/bios16.asm post_code.inc
	nasm $(NASMFLAGS) -f elf32 $< -o $@

qemu_start.o: qemu/stage1/qemu_start.asm
	nasm $(NASMFLAGS) -f elf32 $< -o $@

qemu_stage1.o: qemu/stage1/qemu_stage1.c blob.h shared_service/service_table.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

stage3_entry.o: stage3/entry.c stage3/stage3.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

stage3_runtime.o: stage3/stage3.c stage3/stage3.h post_code.h bios_benchmark.h bios_io.h bios_legacy.h bios_linux.h bios_memtest.h bios_memory.h bios_selftest.h bios_serial.h bios_settings.h bios_shadow.h bios_stage_context.h bios_storage.h app/legacy/legacy_floppy.h app/legacy/legacy_thunk.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

bios_acpi_runtime.o: bios_acpi_runtime.c bios_acpi_runtime.h acpi_tables.h bios_io.h bios_serial.h shared_service/service_table.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

bios_benchmark.o: bios_benchmark.c bios_benchmark.h bios_serial.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

bios_legacy.o: bios_legacy.c bios_legacy.h bios_memory.h bios_rtc.h bios_serial.h bios_storage.h app/legacy/legacy_boot.h app/legacy/legacy_floppy.h app/legacy/legacy_platform.h app/legacy/legacy_runtime.h app/legacy/legacy_thunk.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

bios_linux.o: bios_linux.c bios_linux.h bios_acpi_runtime.h bios_memory.h bios_nvram.h bios_rtc.h bios_serial.h bios_settings.h bios_stage_context.h bios_storage.h app/linux_loader/linux_loader.h app/legacy/legacy_thunk.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

bios_maintenance.o: bios_maintenance.c bios_maintenance.h bios_io.h bios_nvram.h bios_serial.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

bios_memtest.o: bios_memtest.c bios_memtest.h bios_io.h bios_memory.h bios_serial.h post_code.h shared_service/service_table.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

bios_memory.o: bios_memory.c bios_memory.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

bios_nvram.o: bios_nvram.c bios_nvram.h bios_io.h bios_pci.h bios_serial.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

bios_rtc.o: bios_rtc.c bios_io.h bios_rtc.h bios_serial.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

bios_selftest.o: bios_selftest.c bios_selftest.h bios_linux.h bios_serial.h bios_stage_context.h bios_storage.h app/linux_loader/linux_loader.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

bios_serial.o: bios_serial.c bios_io.h bios_serial.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

bios_settings.o: bios_settings.c bios_settings.h bios_maintenance.h bios_nvram.h bios_serial.h bios_storage.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

bios_shadow.o: bios_shadow.c bios_shadow.h blob.h bios_io.h bios_pci.h bios_serial.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

bios_stage_context.o: bios_stage_context.c bios_stage_context.h blob.h shared_service/service_table.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

bios_pci.o: bios_pci.c bios_io.h bios_pci.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

bios_storage.o: bios_storage.c post_code.h bios_io.h bios_pci.h bios_serial.h bios_storage.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

linux_loader.o: app/linux_loader/linux_loader.c app/linux_loader/linux_loader.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

legacy_floppy.o: app/legacy/legacy_floppy.c app/legacy/legacy_floppy.h app/legacy/legacy_platform.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

legacy_bda.o: app/legacy/legacy_bda.c app/legacy/legacy_bda.h app/legacy/legacy_keyboard.h app/legacy/legacy_video.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

legacy_boot.o: app/legacy/legacy_boot.c app/legacy/legacy_boot.h app/legacy/legacy_floppy.h app/legacy/legacy_platform.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

legacy_debug.o: app/legacy/legacy_debug.c app/legacy/legacy_debug.h app/legacy/legacy_rm.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

legacy_int13.o: app/legacy/legacy_int13.c app/legacy/legacy_floppy.h app/legacy/legacy_int13.h app/legacy/legacy_platform.h app/legacy/legacy_rm.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

legacy_int15.o: app/legacy/legacy_int15.c app/legacy/legacy_int15.h app/legacy/legacy_platform.h app/legacy/legacy_rm.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

legacy_keyboard.o: app/legacy/legacy_keyboard.c app/legacy/legacy_io.h app/legacy/legacy_keyboard.h app/legacy/legacy_rm.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

legacy_misc.o: app/legacy/legacy_misc.c app/legacy/legacy_misc.h app/legacy/legacy_rm.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

legacy_platform.o: app/legacy/legacy_platform.c app/legacy/legacy_platform.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

legacy_runtime.o: app/legacy/legacy_runtime.c app/legacy/legacy_runtime.h app/legacy/legacy_service.h app/legacy/legacy_thunk.h app/legacy/legacy_timer.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

legacy_service.o: app/legacy/legacy_service.c app/legacy/legacy_boot.h app/legacy/legacy_debug.h app/legacy/legacy_int13.h app/legacy/legacy_int15.h app/legacy/legacy_keyboard.h app/legacy/legacy_misc.h app/legacy/legacy_platform.h app/legacy/legacy_rm.h app/legacy/legacy_service.h app/legacy/legacy_thunk.h app/legacy/legacy_timer.h app/legacy/legacy_time.h app/legacy/legacy_video.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

legacy_thunk.o: app/legacy/legacy_thunk.c app/legacy/legacy_bda.h app/legacy/legacy_thunk.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

legacy_timer.o: app/legacy/legacy_timer.c app/legacy/legacy_io.h app/legacy/legacy_timer.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

legacy_time.o: app/legacy/legacy_time.c app/legacy/legacy_platform.h app/legacy/legacy_rm.h app/legacy/legacy_time.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

legacy_video.o: app/legacy/legacy_video.c app/legacy/legacy_platform.h app/legacy/legacy_rm.h app/legacy/legacy_video.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

romtest: romtest.asm
	nasm -f bin $< -o $@

test: stdtest.img usbmbr.img linuxprobe.img linuxprobe_raw.img qemu_flat_test_bios.bin qemu_legacy_test_bios.bin
	python3 run_qemu_tests.py

clean:
	rm -f start start_legacy start_legacy_test start.elf start_legacy.elf linuxprobe.elf *.map *.o *.su *.bin *.exe *.com *.obj *.img *.aml *.asl romtest post_code.h post_code.inc

install-all: install-xrecv-exe install-mem-dmp-exe install-flashutil-exe
