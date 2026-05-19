CC := gcc
LD := ld
OBJCOPY := objcopy
LIBGCC := $(shell $(CC) -m32 -print-libgcc-file-name)
CPPFLAGS := -I. -Istage3 -Ilib/acpi -Ilib/x86 -Ilib/pci -Ilib/serial -Ilib/storage
DEPFLAGS := -MMD -MP
CFLAGS := -O2 -m32 -march=pentium2 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fstack-usage -Wstack-usage=256 -nostdlib -nostartfiles -nodefaultlibs -Wall -Wextra $(DEPFLAGS)
NASMFLAGS := -I.
OBJCOPY_ELF_FLAGS := -I binary -O elf32-i386 -B i386
BUILD_SCRIPTS := ../scripts/build
TEST_SCRIPTS := ../scripts/test
CORE_C_OBJS := bootblock.o stage2.o qemu_stage2.o acpi_tables.o blob_service.o \
	l2_service.o qemu_stage1.o stage3_entry.o stage3_runtime.o \
	bios_acpi_runtime.o bios_benchmark.o bios_legacy.o bios_linux.o \
	bios_maintenance.o bios_memtest.o bios_memory.o bios_nvram.o \
	bios_rtc.o bios_selftest.o bios_serial.o bios_settings.o \
	bios_shadow.o bios_stage_context.o bios_pci.o bios_storage.o \
	linux_loader.o legacy_bda.o legacy_boot.o legacy_debug.o \
	legacy_floppy.o legacy_int13.o legacy_int15.o legacy_keyboard.o \
	legacy_misc.o legacy_platform.o legacy_runtime.o legacy_service.o \
	legacy_thunk.o legacy_timer.o legacy_time.o legacy_video.o

COMPILE_C = $(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
COMPILE_UACPI_C = $(CC) $(UACPI_CPPFLAGS) $(UACPI_CFLAGS) -c $< -o $@
P2B_STAGE1_ELF := platform/p2b98_xv/stage1/stage1.elf
P2B_STAGE2_ELF := platform/p2b98_xv/stage2/stage2.elf
QEMU_STAGE1_ELF := platform/qemu/stage1/stage1.elf
QEMU_STAGE2_ELF := platform/qemu/stage2/stage2.elf
STAGE3_DIR_ELF := stage3/stage3.elf
GENROM_STAGE_ELFS := $(P2B_STAGE1_ELF) $(P2B_STAGE2_ELF) \
	$(QEMU_STAGE1_ELF) $(QEMU_STAGE2_ELF) $(STAGE3_DIR_ELF)
GENROM_BINS := p2b98_xv_legacy_genrom.bin p2b98_xv_selftest_genrom.bin \
	p2b98_xv_linux_genrom.bin qemu_legacy_genrom.bin \
	qemu_selftest_genrom.bin qemu_linux_genrom.bin

post_code.h post_code.inc ../specs/POST_CODE.md: post_code.def $(BUILD_SCRIPTS)/gen_post_codes.py
	python3 $(BUILD_SCRIPTS)/gen_post_codes.py

include app/legacy/Makefile
include app/linux_loader/Makefile
include app/selftest/s3test/Makefile
include stage3/Makefile
include platform/p2b98_xv/stage1/Makefile
include platform/p2b98_xv/stage2/Makefile
include platform/qemu/stage1/Makefile
include platform/qemu/stage2/Makefile

C_DEP_OBJS := $(CORE_C_OBJS) $(SELFTEST_C_OBJS)
DEPS := $(C_DEP_OBJS:.o=.d)

fdos_blob.bin: ../freedos_boot_fd.img $(BUILD_SCRIPTS)/gen_blob.py
	python3 $(BUILD_SCRIPTS)/gen_blob.py --fat12-normalize ../freedos_boot_fd.img fdos_blob.bin

fdos_flat_test_blob.bin: custflat.img $(BUILD_SCRIPTS)/gen_blob.py
	python3 $(BUILD_SCRIPTS)/gen_blob.py --fat12-normalize custflat.img fdos_flat_test_blob.bin

fdos_blob.o: fdos_blob.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.fdos,alloc,load,readonly,data,contents \
		$< $@

fdos_flat_test_blob.o: fdos_flat_test_blob.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.fdos,alloc,load,readonly,data,contents \
		$< $@

dsdt.aml: ../specs/dsdt.dsl $(BUILD_SCRIPTS)/gen_dsdt_aml.py
	python3 $(BUILD_SCRIPTS)/gen_dsdt_aml.py ../specs/dsdt.dsl dsdt.aml

dsdt_blob.bin: dsdt.aml $(BUILD_SCRIPTS)/gen_blob.py
	python3 $(BUILD_SCRIPTS)/gen_blob.py dsdt.aml dsdt_blob.bin

dsdt_blob.o: dsdt_blob.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.dsdtblob,alloc,load,readonly,data,contents \
		$< $@

vgabios_blob.bin: ../images/gbsmc.088 $(BUILD_SCRIPTS)/gen_blob.py
	python3 $(BUILD_SCRIPTS)/gen_blob.py ../images/gbsmc.088 vgabios_blob.bin

vgabios_blob.o: vgabios_blob.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.vgabiosblob,alloc,load,readonly,data,contents \
		$< $@

testfd.img: $(BUILD_SCRIPTS)/gen_testfd.py
	python3 $(BUILD_SCRIPTS)/gen_testfd.py

int13probe.bin: int13probe.asm
	nasm -f bin $< -o $@

int13probe.img: int13probe.bin $(BUILD_SCRIPTS)/gen_int13probe.py
	python3 $(BUILD_SCRIPTS)/gen_int13probe.py

usbmbr.bin: usbmbr.asm
	nasm -f bin $< -o $@

usbmbr.img: usbmbr.bin
	cp $< $@
	truncate -s 1474560 $@

linuxprobe.o: linuxprobe.asm
	nasm -f elf32 $< -o $@

linuxprobe.elf: linuxprobe.o linuxprobe.ld
	$(LD) -m elf_i386 -T linuxprobe.ld -o $@ linuxprobe.o

linuxprobe.img linuxprobe_raw.img: linuxprobe.elf $(BUILD_SCRIPTS)/gen_linuxprobe_img.py
	python3 $(BUILD_SCRIPTS)/gen_linuxprobe_img.py

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

flashtest.img: flashutil.exe $(BUILD_SCRIPTS)/gen_flashtest_img.py
	python3 $(BUILD_SCRIPTS)/gen_flashtest_img.py

biostest.exe: ../tools/dos/biostest.c
	WATCOM=/opt/watcom INCLUDE=/opt/watcom/h PATH=/opt/watcom/binl64:$$PATH /opt/watcom/binl64/owcc -b dos -mcmodel=s -os -s $< -o $@

flattest.exe: ../tools/dos/flattest.c
	WATCOM=/opt/watcom INCLUDE=/opt/watcom/h PATH=/opt/watcom/binl64:$$PATH /opt/watcom/binl64/owcc -b dos -mcmodel=s -os -s $< -o $@

shutdown.exe: ../tools/dos/shutdown.c
	WATCOM=/opt/watcom INCLUDE=/opt/watcom/h PATH=/opt/watcom/binl64:$$PATH /opt/watcom/binl64/owcc -b dos -mcmodel=s -os -s $< -o $@

stdtest.img custbio.img custflat.img: biostest.exe flattest.exe shutdown.exe flashutil.exe $(BUILD_SCRIPTS)/gen_test_images.py
	python3 $(BUILD_SCRIPTS)/gen_test_images.py

test10.img: $(BUILD_SCRIPTS)/gen_test10.py
	python3 $(BUILD_SCRIPTS)/gen_test10.py

test10_sparse.bin: test10.img $(BUILD_SCRIPTS)/gen_fdos_sparse.py
	python3 $(BUILD_SCRIPTS)/gen_fdos_sparse.py test10.img test10_sparse.bin

test10_sparse.o: test10_sparse.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.test10,alloc,load,readonly,data,contents \
		$< $@

testfd_sparse.bin: testfd.img $(BUILD_SCRIPTS)/gen_fdos_sparse.py
	python3 $(BUILD_SCRIPTS)/gen_fdos_sparse.py testfd.img testfd_sparse.bin

testfd_sparse.o: testfd_sparse.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.testfd,alloc,load,readonly,data,contents \
		$< $@

test_floppy_blob.bin: testfd.img $(BUILD_SCRIPTS)/gen_fdos_sparse.py
	python3 $(BUILD_SCRIPTS)/gen_fdos_sparse.py testfd.img test_floppy_blob.bin

stage3.bin: stage3.elf
	$(OBJCOPY) -O binary $< $@

bios.bin: stage3.bin
	cp $< $@

stage3_blob.bin: stage3.bin $(BUILD_SCRIPTS)/gen_blob.py
	python3 $(BUILD_SCRIPTS)/gen_blob.py --load-addr 0x00200000 stage3.bin stage3_blob.bin

bios_blob.bin: stage3_blob.bin
	cp $< $@

bios_blob.o: bios_blob.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.biosblob,alloc,load,readonly,data,contents \
		$< $@

stage2.bin: stage2.elf
	$(OBJCOPY) -O binary $< $@

stage2_blob.bin: stage2.bin $(BUILD_SCRIPTS)/gen_blob.py
	python3 $(BUILD_SCRIPTS)/gen_blob.py --load-addr 0x00080000 stage2.bin stage2_blob.bin

stage2_blob.o: stage2_blob.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.stage2blob,alloc,load,readonly,data,contents \
		$< $@

qemu_stage2.bin: qemu_stage2.elf
	$(OBJCOPY) -O binary $< $@

qemu_stage2_blob.bin: qemu_stage2.bin $(BUILD_SCRIPTS)/gen_blob.py
	python3 $(BUILD_SCRIPTS)/gen_blob.py --load-addr 0x00080000 qemu_stage2.bin qemu_stage2_blob.bin

qemu_stage2_blob.o: qemu_stage2_blob.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.stage2blob,alloc,load,readonly,data,contents \
		$< $@

start: start.elf
	$(OBJCOPY) -O binary --pad-to 0x100000 --gap-fill 0x90 $< $@

start.elf: start_asm.o bootblock.o blob_service.o stage2_blob.o bios_blob.o dsdt_blob.o vgabios_blob.o test_elf_blob.o platform/p2b98_xv/stage1/stage1.ld
	$(LD) -m elf_i386 -T platform/p2b98_xv/stage1/stage1.ld -Map start.map -o $@ start_asm.o bootblock.o blob_service.o stage2_blob.o bios_blob.o dsdt_blob.o vgabios_blob.o test_elf_blob.o

bios.elf: stage3.elf
	cp $< $@

qemu_bios.bin: qemu_start.elf
	$(OBJCOPY) -O binary $< $@
	truncate -s 262144 $@

qemu_start.elf: qemu_start.o qemu_stage1.o blob_service.o qemu_stage2_blob.o bios_blob.o test_elf_blob.o platform/qemu/stage1/stage1.ld
	$(LD) -m elf_i386 -T platform/qemu/stage1/stage1.ld -Map qemu.map -o $@ qemu_start.o qemu_stage1.o blob_service.o qemu_stage2_blob.o bios_blob.o test_elf_blob.o

qemu_legacy_bios.bin: qemu_legacy_start.elf
	$(OBJCOPY) -O binary $< $@
	truncate -s 262144 $@

qemu_legacy_start.elf: qemu_start.o qemu_stage1.o blob_service.o qemu_stage2_blob.o bios_blob.o platform/qemu/stage1/stage1.ld
	$(LD) -m elf_i386 -T platform/qemu/stage1/stage1.ld -Map qemu_legacy.map -o $@ qemu_start.o qemu_stage1.o blob_service.o qemu_stage2_blob.o bios_blob.o

qemu_flat_test_bios.bin: qemu_flat_test_start.elf
	$(OBJCOPY) -O binary $< $@
	truncate -s 262144 $@

qemu_flat_test_start.elf: qemu_start.o qemu_stage1.o blob_service.o qemu_stage2_blob.o bios_blob.o test_elf_blob.o platform/qemu/stage1/stage1.ld
	$(LD) -m elf_i386 -T platform/qemu/stage1/stage1.ld -Map qemu_flat_test.map -o $@ qemu_start.o qemu_stage1.o blob_service.o qemu_stage2_blob.o bios_blob.o test_elf_blob.o

qemu_legacy_test_bios.bin: qemu_legacy_bios.bin test_floppy_blob.bin $(BUILD_SCRIPTS)/patch_rom_free.py
	cp qemu_legacy_bios.bin $@
	python3 $(BUILD_SCRIPTS)/patch_rom_free.py $@ test_floppy_blob.bin

genrom: $(GENROM_BINS)

p2b98_xv_%_genrom.bin: platform/p2b98_xv/%.blobs $(GENROM_STAGE_ELFS) dsdt.aml ../images/gbsmc.088 $(BUILD_SCRIPTS)/gen_rom.py $(BUILD_SCRIPTS)/gen_blob.py
	python3 $(BUILD_SCRIPTS)/gen_rom.py $< $@

qemu_%_genrom.bin: platform/qemu/%.blobs $(GENROM_STAGE_ELFS) dsdt.aml ../images/gbsmc.088 $(BUILD_SCRIPTS)/gen_rom.py $(BUILD_SCRIPTS)/gen_blob.py
	python3 $(BUILD_SCRIPTS)/gen_rom.py $< $@

start_legacy: start_asm.o bootblock.o blob_service.o stage2_blob.o bios_blob.o platform/p2b98_xv/stage1/stage1.ld
	$(LD) -m elf_i386 -T platform/p2b98_xv/stage1/stage1.ld -Map start_legacy.map -o start_legacy.elf start_asm.o bootblock.o blob_service.o stage2_blob.o bios_blob.o
	$(OBJCOPY) -O binary --pad-to 0x100000 --gap-fill 0x90 start_legacy.elf $@

start_legacy_test: start_legacy test_floppy_blob.bin $(BUILD_SCRIPTS)/patch_rom_free.py
	cp start_legacy $@
	python3 $(BUILD_SCRIPTS)/patch_rom_free.py $@ test_floppy_blob.bin

start_asm.o: platform/p2b98_xv/stage1/start.asm post_code.inc
	nasm $(NASMFLAGS) -f elf32 $< -o $@

bootblock.o: platform/p2b98_xv/stage1/bootblock.c
	$(COMPILE_C)

stage2.o: platform/p2b98_xv/stage2/stage2.c
	$(COMPILE_C)

qemu_stage2.o: platform/qemu/stage2/stage2.c
	$(COMPILE_C)

acpi_tables.o: lib/acpi/acpi_tables.c
	$(COMPILE_C)

blob_service.o: shared_service/blob_service.c
	$(COMPILE_C)

l2_service.o: platform/p2b98_xv/stage2/l2_service.c
	$(COMPILE_C)

qemu_start.o: platform/qemu/stage1/qemu_start.asm
	nasm $(NASMFLAGS) -f elf32 $< -o $@

qemu_stage1.o: platform/qemu/stage1/qemu_stage1.c
	$(COMPILE_C)

stage3_entry.o: stage3/entry.c
	$(COMPILE_C)

stage3_runtime.o: stage3/stage3.c
	$(COMPILE_C)

bios_acpi_runtime.o: stage3/bios_acpi_runtime.c
	$(COMPILE_C)

bios_benchmark.o: stage3/bios_benchmark.c
	$(COMPILE_C)

bios_legacy.o: stage3/bios_legacy.c
	$(COMPILE_C)

bios_linux.o: stage3/bios_linux.c
	$(COMPILE_C)

bios_maintenance.o: stage3/bios_maintenance.c
	$(COMPILE_C)

bios_memtest.o: stage3/bios_memtest.c
	$(COMPILE_C)

bios_memory.o: lib/x86/bios_memory.c
	$(COMPILE_C)

bios_nvram.o: lib/x86/bios_nvram.c
	$(COMPILE_C)

bios_rtc.o: lib/x86/bios_rtc.c
	$(COMPILE_C)

bios_selftest.o: stage3/bios_selftest.c
	$(COMPILE_C)

bios_serial.o: lib/serial/bios_serial.c
	$(COMPILE_C)

bios_settings.o: stage3/bios_settings.c
	$(COMPILE_C)

bios_shadow.o: stage3/bios_shadow.c
	$(COMPILE_C)

bios_stage_context.o: stage3/bios_stage_context.c
	$(COMPILE_C)

bios_pci.o: lib/pci/bios_pci.c
	$(COMPILE_C)

bios_storage.o: lib/storage/bios_storage.c
	$(COMPILE_C)

romtest: romtest.asm
	nasm -f bin $< -o $@

test: stdtest.img usbmbr.img linuxprobe.img linuxprobe_raw.img qemu_flat_test_bios.bin qemu_legacy_test_bios.bin qemu_legacy_genrom.bin
	python3 $(TEST_SCRIPTS)/run_qemu_tests.py

-include $(DEPS)

clean:
	rm -f start start_legacy start_legacy_test start.elf start_legacy.elf linuxprobe.elf *.map *.o *.d *.su *.bin *.exe *.com *.obj *.img *.aml *.asl romtest post_code.h post_code.inc $(GENROM_STAGE_ELFS) platform/p2b98_xv/stage1/*.map platform/qemu/stage1/*.map

install-all: install-xrecv-exe install-mem-dmp-exe install-flashutil-exe
