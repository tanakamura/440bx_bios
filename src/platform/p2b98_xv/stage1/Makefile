# Included from ../../../Makefile; paths are relative to src/.

P2B_STAGE1_C_OBJS := bootblock.o
P2B_STAGE1_OBJS := start_asm.o $(P2B_STAGE1_C_OBJS) $(SHARED_SERVICE_OBJS)

$(P2B_STAGE1_ELF): $(P2B_STAGE1_OBJS) platform/p2b98_xv/stage1/stage1.ld
	$(LD) -m elf_i386 -T platform/p2b98_xv/stage1/stage1.ld -Map platform/p2b98_xv/stage1/stage1.map -o $@ $(P2B_STAGE1_OBJS)

start: start.elf
	$(OBJCOPY) -O binary --pad-to 0x100000 --gap-fill 0x90 $< $@

start.elf: start_asm.o bootblock.o blob_service.o stage2_blob.o bios_blob.o dsdt_blob.o vgabios_blob.o test_elf_blob.o platform/p2b98_xv/stage1/stage1.ld
	$(LD) -m elf_i386 -T platform/p2b98_xv/stage1/stage1.ld -Map start.map -o $@ start_asm.o bootblock.o blob_service.o stage2_blob.o bios_blob.o dsdt_blob.o vgabios_blob.o test_elf_blob.o

start_legacy: start_asm.o bootblock.o blob_service.o stage2_blob.o bios_blob.o platform/p2b98_xv/stage1/stage1.ld
	$(LD) -m elf_i386 -T platform/p2b98_xv/stage1/stage1.ld -Map start_legacy.map -o start_legacy.elf start_asm.o bootblock.o blob_service.o stage2_blob.o bios_blob.o
	$(OBJCOPY) -O binary --pad-to 0x100000 --gap-fill 0x90 start_legacy.elf $@

start_legacy_test: start_legacy test_floppy_blob.bin $(BUILD_SCRIPTS)/patch_rom_free.py
	cp start_legacy $@
	python3 $(BUILD_SCRIPTS)/patch_rom_free.py $@ test_floppy_blob.bin

start_asm.o: platform/p2b98_xv/stage1/start.asm post_code.inc
	nasm $(NASMFLAGS) -f elf32 $< -o $@

bootblock.o: platform/p2b98_xv/stage1/bootblock.c
	$(COMPILE_C)
