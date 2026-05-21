CC := gcc
LD := ld
OBJCOPY := objcopy
LIBGCC := $(shell $(CC) -m32 -print-libgcc-file-name)
CPPFLAGS := -I. -Iinclude -Istage3 -Ilib/acpi -Ilib/x86 -Ilib/pci -Ilib/serial -Ilib/storage
DEPFLAGS := -MMD -MP
CFLAGS := -O2 -m32 -march=pentium2 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -ffunction-sections -fdata-sections -fstack-usage -Wstack-usage=256 -nostdlib -nostartfiles -nodefaultlibs -Wall -Wextra $(DEPFLAGS)
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
LEGACY_APP_ELF := app/legacy/legacy_app.elf
LINUX_LOADER_APP_ELF := app/linux_loader/linux_loader_app.elf
SELFTEST_ELF := app/selftest/s3test/s3test.elf
GENROM_STAGE_ELFS := $(P2B_STAGE1_ELF) $(P2B_STAGE2_ELF) \
	$(QEMU_STAGE1_ELF) $(QEMU_STAGE2_ELF) $(STAGE3_DIR_ELF)
BUILD_ARTIFACT_DIRS := app/legacy app/linux_loader app/selftest/s3test \
	lib/acpi lib/pci lib/serial lib/storage lib/x86 shared_service stage3 \
	platform/p2b98_xv/stage1 platform/p2b98_xv/stage2 \
	platform/qemu/stage1 platform/qemu/stage2

post_code.h post_code.inc ../specs/POST_CODE.md: post_code.def $(BUILD_SCRIPTS)/gen_post_codes.py
	python3 $(BUILD_SCRIPTS)/gen_post_codes.py

shared_service/service_table.inc: shared_service/service_table.h $(BUILD_SCRIPTS)/gen_shared_service_asm.py
	python3 $(BUILD_SCRIPTS)/gen_shared_service_asm.py

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

dsdt.aml: ../specs/dsdt.dsl $(BUILD_SCRIPTS)/gen_dsdt_aml.py
	python3 $(BUILD_SCRIPTS)/gen_dsdt_aml.py ../specs/dsdt.dsl dsdt.aml

testfd.img: $(BUILD_SCRIPTS)/gen_testfd.py
	python3 $(BUILD_SCRIPTS)/gen_testfd.py

int13probe.bin: tests/boot/int13probe.asm
	nasm -f bin $< -o $@

int13probe.img: int13probe.bin $(BUILD_SCRIPTS)/gen_int13probe.py
	python3 $(BUILD_SCRIPTS)/gen_int13probe.py

usbmbr.bin: tests/boot/usbmbr.asm
	nasm -f bin $< -o $@

usbmbr.img: usbmbr.bin
	cp $< $@
	truncate -s 1474560 $@

linuxprobe.o: tests/boot/linuxprobe.asm
	nasm -f elf32 $< -o $@

linuxprobe.elf: linuxprobe.o tests/boot/linuxprobe.ld
	$(LD) -m elf_i386 -T tests/boot/linuxprobe.ld -o $@ linuxprobe.o

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

test_floppy_blob.bin: testfd.img $(BUILD_SCRIPTS)/gen_fdos_sparse.py
	python3 $(BUILD_SCRIPTS)/gen_fdos_sparse.py testfd.img test_floppy_blob.bin

bios.elf: $(STAGE3_DIR_ELF)
	cp $< $@

genrom: $(GENROM_BINS)

romtest: tests/board/romtest.asm
	nasm -f bin $< -o $@

test: stdtest.img usbmbr.img linuxprobe.img linuxprobe_raw.img qemu_legacy_test_bios.bin qemu_legacy_genrom.bin qemu_linux_genrom.bin
	python3 $(TEST_SCRIPTS)/run_qemu_tests.py

-include $(DEPS)

clean:
	rm -f start start_legacy start_legacy_test start.elf start_legacy.elf linuxprobe.elf *.map *.o *.d *.su *.bin *.exe *.com *.obj *.img *.aml *.asl romtest post_code.h post_code.inc shared_service/service_table.inc $(GENROM_STAGE_ELFS) $(LEGACY_APP_ELF) $(LINUX_LOADER_APP_ELF) $(SELFTEST_ELF) $(addsuffix /*.o,$(BUILD_ARTIFACT_DIRS)) $(addsuffix /*.d,$(BUILD_ARTIFACT_DIRS)) $(addsuffix /*.su,$(BUILD_ARTIFACT_DIRS)) $(addsuffix /*.map,$(BUILD_ARTIFACT_DIRS)) app/legacy/*.bin app/linux_loader/*.bin

install-all: install-xrecv-exe install-mem-dmp-exe install-flashutil-exe
