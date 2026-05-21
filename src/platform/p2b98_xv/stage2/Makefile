# Included from ../../../Makefile; paths are relative to src/.

P2B_STAGE2_C_OBJS := platform/p2b98_xv/stage2/stage2.o \
	platform/p2b98_xv/stage2/l2_service.o
P2B_STAGE2_OBJS := $(P2B_STAGE2_C_OBJS) $(ACPI_OBJS) $(SHARED_SERVICE_OBJS)

$(P2B_STAGE2_ELF): $(P2B_STAGE2_OBJS) $(SHARED_SERVICE_RELOC_CHECK) platform/p2b98_xv/stage2/stage2.ld include/blob.ld
	$(LD) -m elf_i386 -T platform/p2b98_xv/stage2/stage2.ld -Map platform/p2b98_xv/stage2/stage2.map -o $@ $(P2B_STAGE2_OBJS)
