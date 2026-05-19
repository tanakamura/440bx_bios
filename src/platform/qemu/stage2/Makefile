# Included from ../../../Makefile; paths are relative to src/.

QEMU_STAGE2_OBJS := qemu_stage2.o acpi_tables.o

qemu_stage2.elf: $(QEMU_STAGE2_OBJS) platform/qemu/stage2/stage2.ld
	$(LD) -m elf_i386 -T platform/qemu/stage2/stage2.ld -Map qemu_stage2.map -o $@ $(QEMU_STAGE2_OBJS)

$(QEMU_STAGE2_ELF): qemu_stage2.elf
	cp $< $@

qemu_stage2.o: platform/qemu/stage2/stage2.c
	$(COMPILE_C)
