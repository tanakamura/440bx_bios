# Included from ../../../Makefile; paths are relative to src/.

QEMU_STAGE2_C_OBJS := platform/qemu/stage2/qemu_stage2.o
QEMU_STAGE2_OBJS := $(QEMU_STAGE2_C_OBJS) $(ACPI_OBJS)

$(QEMU_STAGE2_ELF): $(QEMU_STAGE2_OBJS) platform/qemu/stage2/stage2.ld
	$(LD) -m elf_i386 -T platform/qemu/stage2/stage2.ld -Map platform/qemu/stage2/stage2.map -o $@ $(QEMU_STAGE2_OBJS)

platform/qemu/stage2/qemu_stage2.o: platform/qemu/stage2/stage2.c
	$(COMPILE_C)
