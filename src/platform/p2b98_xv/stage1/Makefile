# Included from ../../../Makefile; paths are relative to src/.

P2B_STAGE1_C_OBJS := platform/p2b98_xv/stage1/bootblock.o
P2B_STAGE1_OBJS := platform/p2b98_xv/stage1/start.o $(P2B_STAGE1_C_OBJS) \
	$(SHARED_SERVICE_OBJS)

$(P2B_STAGE1_ELF): $(P2B_STAGE1_OBJS) platform/p2b98_xv/stage1/stage1.ld
	$(LD) -m elf_i386 -T platform/p2b98_xv/stage1/stage1.ld -Map platform/p2b98_xv/stage1/stage1.map -o $@ $(P2B_STAGE1_OBJS)

start: p2b98_xv_linux_genrom.bin
	cp $< $@

start_legacy: p2b98_xv_legacy_genrom.bin
	cp $< $@

start_legacy_test: start_legacy test_floppy_blob.bin $(BUILD_SCRIPTS)/patch_rom_free.py
	cp start_legacy $@
	python3 $(BUILD_SCRIPTS)/patch_rom_free.py $@ test_floppy_blob.bin

platform/p2b98_xv/stage1/start.o: platform/p2b98_xv/stage1/start.asm post_code.inc
	nasm $(NASMFLAGS) -f elf32 $< -o $@

platform/p2b98_xv/stage1/bootblock.o: platform/p2b98_xv/stage1/bootblock.c
	$(COMPILE_C)
