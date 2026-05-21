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
COMPILE_C = $(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
COMPILE_UACPI_C = $(CC) $(UACPI_CPPFLAGS) $(UACPI_CFLAGS) -c $< -o $@
P2B_STAGE1_ELF := platform/p2b98_xv/stage1/stage1.elf
P2B_STAGE2_ELF := platform/p2b98_xv/stage2/stage2.elf
QEMU_STAGE1_ELF := platform/qemu/stage1/stage1.elf
QEMU_STAGE2_ELF := platform/qemu/stage2/stage2.elf
STAGE3_DIR_ELF := stage3/stage3.elf
GENROM_STAGE_ELFS := $(P2B_STAGE1_ELF) $(P2B_STAGE2_ELF) \
	$(QEMU_STAGE1_ELF) $(QEMU_STAGE2_ELF) $(STAGE3_DIR_ELF)

post_code.h post_code.inc ../specs/POST_CODE.md: post_code.def $(BUILD_SCRIPTS)/gen_post_codes.py
	python3 $(BUILD_SCRIPTS)/gen_post_codes.py

include app/legacy/Makefile
include app/linux_loader/Makefile
include app/selftest/s3test/Makefile
include lib/acpi/Makefile
include lib/pci/Makefile
include lib/serial/Makefile
include lib/storage/Makefile
include lib/x86/Makefile
include shared_service/Makefile
include stage3/Makefile
include platform/p2b98_xv/Makefile
include platform/p2b98_xv/stage1/Makefile
include platform/p2b98_xv/stage2/Makefile
include platform/qemu/Makefile
include platform/qemu/stage1/Makefile
include platform/qemu/stage2/Makefile

GENROM_BINS := $(P2B98_XV_GENROM_BINS) $(QEMU_GENROM_BINS)
CORE_C_OBJS := $(P2B_STAGE1_C_OBJS) $(P2B_STAGE2_C_OBJS) \
	$(QEMU_STAGE1_C_OBJS) $(QEMU_STAGE2_C_OBJS) \
	$(ACPI_OBJS) $(SHARED_SERVICE_OBJS) $(X86_OBJS) \
	$(SERIAL_OBJS) $(PCI_OBJS) $(STORAGE_OBJS) \
	$(STAGE3_C_OBJS) $(LINUX_LOADER_C_OBJS) $(LEGACY_C_OBJS)
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

bios.elf: stage3.elf
	cp $< $@

genrom: $(GENROM_BINS)

romtest: romtest.asm
	nasm -f bin $< -o $@

test: stdtest.img usbmbr.img linuxprobe.img linuxprobe_raw.img qemu_legacy_test_bios.bin qemu_legacy_genrom.bin qemu_linux_genrom.bin
	python3 $(TEST_SCRIPTS)/run_qemu_tests.py

-include $(DEPS)

clean:
	rm -f start start_legacy start_legacy_test start.elf start_legacy.elf linuxprobe.elf *.map *.o *.d *.su *.bin *.exe *.com *.obj *.img *.aml *.asl romtest post_code.h post_code.inc $(GENROM_STAGE_ELFS) platform/p2b98_xv/stage1/*.map platform/qemu/stage1/*.map

install-all: install-xrecv-exe install-mem-dmp-exe install-flashutil-exe
