# Included from ../../Makefile; paths are relative to src/.

LEGACY_OBJS := bios16.o legacy_bda.o legacy_boot.o legacy_debug.o \
	legacy_floppy.o legacy_int13.o legacy_int15.o legacy_keyboard.o \
	legacy_misc.o legacy_platform.o legacy_runtime.o legacy_service.o \
	legacy_thunk.o legacy_timer.o legacy_time.o legacy_video.o
LEGACY_APP_OBJS := legacy_app_entry.o $(LEGACY_OBJS)
LEGACY_C_OBJS := legacy_app_entry.o $(filter-out bios16.o,$(LEGACY_OBJS))

$(LEGACY_APP_ELF): $(LEGACY_APP_OBJS) app/legacy/legacy.ld
	$(LD) -m elf_i386 -T app/legacy/legacy.ld -Map app/legacy/legacy_app.map -o $@ $(LEGACY_APP_OBJS)

app/legacy/legacy_app.bin: $(LEGACY_APP_ELF)
	$(OBJCOPY) -O binary $< $@

bios16.o: app/legacy/bios16.asm post_code.inc
	nasm $(NASMFLAGS) -f elf32 $< -o $@

legacy_app_entry.o: app/legacy/entry.c
	$(COMPILE_C)

legacy_%.o: app/legacy/legacy_%.c
	$(COMPILE_C)
