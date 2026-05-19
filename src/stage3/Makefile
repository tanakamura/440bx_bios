# Included from ../Makefile; paths are relative to src/.

STAGE3_OBJS := bios16.o stage3_entry.o stage3_runtime.o \
	bios_acpi_runtime.o bios_benchmark.o bios_legacy.o bios_linux.o \
	bios_maintenance.o bios_memtest.o bios_memory.o bios_nvram.o \
	bios_rtc.o bios_selftest.o bios_serial.o bios_settings.o \
	bios_shadow.o bios_stage_context.o bios_pci.o bios_storage.o

STAGE3_APP_OBJS := $(LINUX_LOADER_OBJS) $(LEGACY_OBJS)

stage3.elf: $(STAGE3_OBJS) $(STAGE3_APP_OBJS) stage3/stage3.ld
	$(LD) -m elf_i386 -T stage3/stage3.ld -Map stage3.map -o $@ $(STAGE3_OBJS) $(STAGE3_APP_OBJS)

$(STAGE3_DIR_ELF): stage3.elf
	cp $< $@

stage3_entry.o: stage3/entry.c
	$(COMPILE_C)

stage3_runtime.o: stage3/stage3.c
	$(COMPILE_C)

bios_%.o: stage3/bios_%.c
	$(COMPILE_C)
