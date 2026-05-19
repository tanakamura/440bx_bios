# Included from ../../../Makefile; paths are relative to src/.

QEMU_STAGE1_OBJS := qemu_start.o qemu_stage1.o blob_service.o

$(QEMU_STAGE1_ELF): $(QEMU_STAGE1_OBJS) platform/qemu/stage1/stage1.ld
	$(LD) -m elf_i386 -T platform/qemu/stage1/stage1.ld -Map platform/qemu/stage1/stage1.map -o $@ $(QEMU_STAGE1_OBJS)

qemu_start.o: platform/qemu/stage1/qemu_start.asm
	nasm $(NASMFLAGS) -f elf32 $< -o $@

qemu_stage1.o: platform/qemu/stage1/qemu_stage1.c
	$(COMPILE_C)
