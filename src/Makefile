CC := gcc
LD := ld
OBJCOPY := objcopy
LIBGCC := $(shell $(CC) -m32 -print-libgcc-file-name)
VBIOS_INIT_MODE ?= vm86
IDE_ENABLE_DMA ?= 0
CPPFLAGS := -I. -Iinclude -Istage3 -Ilib/acpi -Ilib/app -Ilib/vm86 -Ilib/x86 -Ilib/pci -Ilib/serial -Ilib/storage
DEPFLAGS := -MMD -MP
CFLAGS := -O2 -m32 -march=pentium2 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -ffunction-sections -fdata-sections -fstack-usage -Wstack-usage=256 -nostdlib -nostartfiles -nodefaultlibs -Wall -Wextra $(DEPFLAGS)
NASMFLAGS := -I.
OBJCOPY_ELF_FLAGS := -I binary -O elf32-i386 -B i386
BUILD_SCRIPTS := ../scripts/build
TEST_SCRIPTS := ../scripts/test
COMPILE_C = $(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
COMPILE_UACPI_C = $(CC) $(UACPI_CPPFLAGS) $(UACPI_CFLAGS) -c $< -o $@
COMPILE_ASM_ELF32 = nasm $(NASMFLAGS) -f elf32 -MD $@.d -MT $@ -MP $< -o $@
COMPILE_ASM_BIN = nasm $(NASMFLAGS) -f bin -MD $@.d -MT $@ -MP $< -o $@

ifeq ($(VBIOS_INIT_MODE),direct)
CPPFLAGS += -DLEGACY_VGABIOS_USE_VM86=0
else
CPPFLAGS += -DLEGACY_VGABIOS_USE_VM86=1
endif
CPPFLAGS += -DBIOS_IDE_ENABLE_DMA=$(IDE_ENABLE_DMA)

%.o: %.c
	$(COMPILE_C)

P2B_STAGE1_ELF := platform/p2b98_xv/stage1/stage1.elf
P2B_STAGE15_ELF := platform/p2b98_xv/stage15/stage15.elf
P2B_STAGE2_ELF := platform/p2b98_xv/stage2/stage2.elf
QEMU_STAGE1_ELF := platform/qemu/stage1/stage1.elf
QEMU_STAGE2_ELF := platform/qemu/stage2/stage2.elf
STAGE3_DIR_ELF := stage3/stage3.elf
LEGACY_APP_ELF := app/legacy/legacy_app.elf
LINUX_LOADER_APP_ELF := app/linux_loader/linux_loader_app.elf
SELFTEST_ELF := app/selftest/s3test/selftest_app.elf
GENROM_STAGE_ELFS := $(P2B_STAGE1_ELF) $(P2B_STAGE15_ELF) $(P2B_STAGE2_ELF) \
	$(QEMU_STAGE1_ELF) $(QEMU_STAGE2_ELF) $(STAGE3_DIR_ELF)
BUILD_ARTIFACT_DIRS := app/legacy app/linux_loader app/selftest/s3test \
	app/selftest/benchmark \
	lib/acpi lib/app lib/pci lib/rm_thunk lib/serial lib/storage lib/vm86 lib/x86 \
	shared_service stage3 \
	platform/p2b98_xv/stage1 platform/p2b98_xv/stage15 \
	platform/p2b98_xv/stage2 \
	platform/qemu/stage1 platform/qemu/stage2

post_code.h post_code.inc ../specs/POST_CODE.md: post_code.def $(BUILD_SCRIPTS)/gen_post_codes.py
	python3 $(BUILD_SCRIPTS)/gen_post_codes.py

shared_service/service_table.inc shared_service/service_table.ld: shared_service/service_table.h $(BUILD_SCRIPTS)/gen_shared_service_asm.py
	python3 $(BUILD_SCRIPTS)/gen_shared_service_asm.py

include/blob.inc include/blob.ld: include/blob.h $(BUILD_SCRIPTS)/gen_blob_asm.py
	python3 $(BUILD_SCRIPTS)/gen_blob_asm.py

include lib/rm_thunk/Makefile
include lib/vm86/Makefile
include app/legacy/Makefile
include app/linux_loader/Makefile
include app/selftest/s3test/Makefile
include app/selftest/benchmark/Makefile
include tests/Makefile
include lib/acpi/Makefile
include lib/app/Makefile
include lib/pci/Makefile
include lib/serial/Makefile
include lib/storage/Makefile
include lib/x86/Makefile
include shared_service/Makefile
include stage3/Makefile
include ../tools/dos/Makefile
include platform/p2b98_xv/Makefile
include platform/p2b98_xv/stage1/Makefile
include platform/p2b98_xv/stage15/Makefile
include platform/p2b98_xv/stage2/Makefile
include platform/qemu/Makefile
include platform/qemu/stage1/Makefile
include platform/qemu/stage2/Makefile

GENROM_BINS := $(P2B98_XV_GENROM_BINS) $(QEMU_GENROM_BINS)
CORE_C_OBJS := $(P2B_STAGE1_C_OBJS) $(P2B_STAGE15_C_OBJS) \
	$(P2B_STAGE2_C_OBJS) \
	$(QEMU_STAGE1_C_OBJS) $(QEMU_STAGE2_C_OBJS) \
	$(ACPI_OBJS) $(APP_COMMON_OBJS) $(SHARED_SERVICE_OBJS) $(VM86_OBJS) $(X86_OBJS) \
	$(SERIAL_OBJS) $(PCI_OBJS) $(STORAGE_OBJS) \
	$(STAGE3_C_OBJS) $(LINUX_LOADER_C_OBJS) $(LEGACY_C_OBJS)
C_DEP_OBJS := $(CORE_C_OBJS) $(SELFTEST_C_OBJS) $(BENCHMARK_C_OBJS)
ASM_DEP_TARGETS := $(RM_THUNK_BIOS16_OBJ) platform/p2b98_xv/stage1/start.o \
	platform/p2b98_xv/stage1/stage15_blob.o \
	platform/p2b98_xv/stage15/stage15_entry.o \
	platform/qemu/stage1/qemu_start.o int13probe.bin usbmbr.bin \
	linuxprobe.o linuxprobe_overlap.o romtest
DEPS := $(C_DEP_OBJS:.o=.d) $(addsuffix .d,$(ASM_DEP_TARGETS))

$(C_DEP_OBJS): | post_code.h

dsdt.aml: ../specs/dsdt.dsl $(BUILD_SCRIPTS)/gen_dsdt_aml.py
	python3 $(BUILD_SCRIPTS)/gen_dsdt_aml.py ../specs/dsdt.dsl dsdt.aml

bios.elf: $(STAGE3_DIR_ELF)
	cp $< $@

genrom: $(GENROM_BINS)

-include $(DEPS)

clean:
	rm -f start start_legacy start_legacy_test start_selftest start_benchmark start.elf start_legacy.elf linuxprobe.elf *.map *.o *.d *.su *.bin *.exe *.com *.obj *.img *.aml *.asl romtest post_code.h post_code.inc include/blob.inc include/blob.ld shared_service/service_table.inc shared_service/service_table.ld $(SHARED_SERVICE_RELOC_CHECK) $(GENROM_STAGE_ELFS) $(LEGACY_APP_ELF) $(LINUX_LOADER_APP_ELF) $(SELFTEST_ELF) $(P2B98_XV_BLOB_DEPFILES) $(QEMU_BLOB_DEPFILES) $(addsuffix /*.o,$(BUILD_ARTIFACT_DIRS)) $(addsuffix /*.d,$(BUILD_ARTIFACT_DIRS)) $(addsuffix /*.su,$(BUILD_ARTIFACT_DIRS)) $(addsuffix /*.map,$(BUILD_ARTIFACT_DIRS)) app/legacy/*.bin app/linux_loader/*.bin app/selftest/s3test/*.bin app/selftest/s3test/*.elf app/selftest/benchmark/*.bin app/selftest/benchmark/*.elf platform/p2b98_xv/stage15/stage15.bin platform/p2b98_xv/stage1/stage15_crc.inc
