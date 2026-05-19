# Included from ../../../Makefile; paths are relative to src/.

P2B_STAGE2_OBJS := stage2.o l2_service.o acpi_tables.o

stage2.elf: $(P2B_STAGE2_OBJS) platform/p2b98_xv/stage2/stage2.ld
	$(LD) -m elf_i386 -T platform/p2b98_xv/stage2/stage2.ld -Map stage2.map -o $@ $(P2B_STAGE2_OBJS)

$(P2B_STAGE2_ELF): stage2.elf
	cp $< $@
