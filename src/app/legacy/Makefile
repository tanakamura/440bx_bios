# Included from ../../Makefile; paths are relative to src/.

LEGACY_OBJS := bios16.o legacy_bda.o legacy_boot.o legacy_debug.o \
	legacy_floppy.o legacy_int13.o legacy_int15.o legacy_keyboard.o \
	legacy_misc.o legacy_platform.o legacy_runtime.o legacy_service.o \
	legacy_thunk.o legacy_timer.o legacy_time.o legacy_video.o

bios16.o: app/legacy/bios16.asm post_code.inc
	nasm $(NASMFLAGS) -f elf32 $< -o $@

legacy_%.o: app/legacy/legacy_%.c
	$(COMPILE_C)
