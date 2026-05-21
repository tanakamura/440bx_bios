# Included from ../../Makefile; paths are relative to src/.

LEGACY_OBJS := app/legacy/bios16.o app/legacy/legacy_bda.o \
	app/legacy/legacy_boot.o app/legacy/legacy_debug.o \
	app/legacy/legacy_floppy.o app/legacy/legacy_int13.o \
	app/legacy/legacy_int15.o app/legacy/legacy_keyboard.o \
	app/legacy/legacy_misc.o app/legacy/legacy_platform.o \
	app/legacy/legacy_runtime.o app/legacy/legacy_service.o \
	app/legacy/legacy_thunk.o app/legacy/legacy_timer.o \
	app/legacy/legacy_time.o app/legacy/legacy_video.o
LEGACY_APP_LIB_OBJS := lib/serial/bios_serial.o lib/pci/bios_pci.o \
	lib/storage/bios_storage.o lib/x86/bios_memory.o lib/x86/bios_rtc.o
LEGACY_APP_OBJS := app/legacy/entry.o $(LEGACY_OBJS) $(LEGACY_APP_LIB_OBJS)
LEGACY_C_OBJS := app/legacy/entry.o $(filter-out app/legacy/bios16.o,$(LEGACY_OBJS))

$(LEGACY_APP_ELF): $(LEGACY_APP_OBJS) app/legacy/legacy.ld
	$(LD) -m elf_i386 -T app/legacy/legacy.ld -Map app/legacy/legacy_app.map -o $@ $(LEGACY_APP_OBJS)

app/legacy/legacy_app.bin: $(LEGACY_APP_ELF)
	$(OBJCOPY) -O binary $< $@

app/legacy/bios16.o: app/legacy/bios16.asm
	$(COMPILE_ASM_ELF32)

app/legacy/entry.o: app/legacy/entry.c
	$(COMPILE_C)

app/legacy/legacy_%.o: app/legacy/legacy_%.c
	$(COMPILE_C)
