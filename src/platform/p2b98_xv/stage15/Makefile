# Included from ../../../Makefile; paths are relative to src/.

P2B_STAGE15_C_OBJS := platform/p2b98_xv/stage15/stage15.o
P2B_STAGE15_OBJS := platform/p2b98_xv/stage15/stage15_entry.o \
	$(P2B_STAGE15_C_OBJS) $(SHARED_SERVICE_OBJS)
P2B_STAGE15_BIN := platform/p2b98_xv/stage15/stage15.bin
P2B_STAGE15_CRC_INC := platform/p2b98_xv/stage1/stage15_crc.inc

$(P2B_STAGE15_ELF): $(P2B_STAGE15_OBJS) $(SHARED_SERVICE_RELOC_CHECK) platform/p2b98_xv/stage15/stage15.ld include/blob.ld
	$(LD) -m elf_i386 -T platform/p2b98_xv/stage15/stage15.ld -Map platform/p2b98_xv/stage15/stage15.map -o $@ $(P2B_STAGE15_OBJS)

$(P2B_STAGE15_BIN): $(P2B_STAGE15_ELF)
	$(OBJCOPY) -O binary $< $@

$(P2B_STAGE15_CRC_INC): $(P2B_STAGE15_BIN) $(BUILD_SCRIPTS)/gen_binary_crc_inc.py
	python3 $(BUILD_SCRIPTS)/gen_binary_crc_inc.py $< $@

platform/p2b98_xv/stage15/stage15_entry.o: platform/p2b98_xv/stage15/stage15_entry.asm include/blob.inc
	$(COMPILE_ASM_ELF32)
