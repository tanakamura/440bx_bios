# Included from ../../../Makefile; paths are relative to src/.

P2B_STAGE1_C_OBJS := bootblock.o
P2B_STAGE1_OBJS := start_asm.o $(P2B_STAGE1_C_OBJS) $(SHARED_SERVICE_OBJS)

$(P2B_STAGE1_ELF): $(P2B_STAGE1_OBJS) platform/p2b98_xv/stage1/stage1.ld
	$(LD) -m elf_i386 -T platform/p2b98_xv/stage1/stage1.ld -Map platform/p2b98_xv/stage1/stage1.map -o $@ $(P2B_STAGE1_OBJS)

start_asm.o: platform/p2b98_xv/stage1/start.asm post_code.inc
	nasm $(NASMFLAGS) -f elf32 $< -o $@

bootblock.o: platform/p2b98_xv/stage1/bootblock.c
	$(COMPILE_C)
