# Included from ../Makefile; paths are relative to src/.

STAGE3_CORE_OBJS := stage3/entry.o stage3/stage3.o \
	stage3/bios_acpi_runtime.o stage3/bios_benchmark.o \
	stage3/bios_legacy.o stage3/bios_linux.o stage3/bios_maintenance.o \
	stage3/bios_memtest.o stage3/bios_selftest.o stage3/bios_settings.o \
	stage3/bios_shadow.o stage3/bios_stage_context.o
STAGE3_LIB_OBJS := $(X86_OBJS) $(SERIAL_OBJS) $(PCI_OBJS) $(STORAGE_OBJS)
STAGE3_OBJS := $(STAGE3_CORE_OBJS) $(STAGE3_LIB_OBJS)
STAGE3_C_OBJS := $(STAGE3_CORE_OBJS)

STAGE3_LEGACY_THUNK_OBJS := app/legacy/bios16.o app/legacy/legacy_thunk.o
STAGE3_APP_OBJS := $(STAGE3_LEGACY_THUNK_OBJS)

$(STAGE3_DIR_ELF): $(STAGE3_OBJS) $(STAGE3_APP_OBJS) stage3/stage3.ld
	$(LD) -m elf_i386 --gc-sections -T stage3/stage3.ld -Map stage3/stage3.map -o $@ $(STAGE3_OBJS) $(STAGE3_APP_OBJS)

stage3/entry.o: stage3/entry.c
	$(COMPILE_C)

stage3/stage3.o: stage3/stage3.c
	$(COMPILE_C)

stage3/bios_%.o: stage3/bios_%.c
	$(COMPILE_C)
