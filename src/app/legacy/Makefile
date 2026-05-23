# Included from ../../Makefile; paths are relative to src/.

LEGACY_OBJS := $(RM_THUNK_BIOS16_OBJ) app/legacy/legacy_bda.o \
	app/legacy/legacy_boot.o app/legacy/legacy_debug.o \
	app/legacy/legacy_floppy.o app/legacy/legacy_int13.o \
	app/legacy/legacy_int15.o app/legacy/legacy_keyboard.o \
	app/legacy/legacy_misc.o app/legacy/legacy_platform.o \
	app/legacy/legacy_runtime.o app/legacy/legacy_service.o \
	app/legacy/legacy_thunk.o app/legacy/legacy_timer.o \
	app/legacy/legacy_time.o app/legacy/legacy_vgabios_vm86.o \
	app/legacy/legacy_video.o
LEGACY_APP_LIB_OBJS := lib/serial/bios_serial.o lib/pci/bios_pci.o \
	lib/storage/bios_storage.o lib/x86/bios_memory.o lib/x86/bios_nvram.o \
	lib/x86/bios_rtc.o lib/app/app_shadow.o lib/app/app_runtime.o \
	$(VM86_OBJS)
LEGACY_APP_OBJS := app/legacy/entry.o $(LEGACY_OBJS) $(LEGACY_APP_LIB_OBJS)
LEGACY_C_OBJS := app/legacy/entry.o $(filter-out $(RM_THUNK_BIOS16_OBJ),$(LEGACY_OBJS))

$(LEGACY_APP_ELF): $(LEGACY_APP_OBJS) app/legacy/legacy.ld include/blob.ld
	$(LD) -m elf_i386 --gc-sections -T app/legacy/legacy.ld -Map app/legacy/legacy_app.map -o $@ $(LEGACY_APP_OBJS)

app/legacy/legacy_app.bin: $(LEGACY_APP_ELF)
	$(OBJCOPY) -O binary $< $@
