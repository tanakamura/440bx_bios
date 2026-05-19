# Included from ../../../Makefile; paths are relative to src/.

QEMU_STAGE1_C_OBJS := qemu_stage1.o
QEMU_STAGE1_OBJS := qemu_start.o $(QEMU_STAGE1_C_OBJS) $(SHARED_SERVICE_OBJS)

$(QEMU_STAGE1_ELF): $(QEMU_STAGE1_OBJS) platform/qemu/stage1/stage1.ld
	$(LD) -m elf_i386 -T platform/qemu/stage1/stage1.ld -Map platform/qemu/stage1/stage1.map -o $@ $(QEMU_STAGE1_OBJS)

qemu_bios.bin: qemu_start.elf
	$(OBJCOPY) -O binary $< $@
	truncate -s 262144 $@

qemu_start.elf: qemu_start.o qemu_stage1.o blob_service.o qemu_stage2_blob.o bios_blob.o test_elf_blob.o platform/qemu/stage1/stage1.ld
	$(LD) -m elf_i386 -T platform/qemu/stage1/stage1.ld -Map qemu.map -o $@ qemu_start.o qemu_stage1.o blob_service.o qemu_stage2_blob.o bios_blob.o test_elf_blob.o

qemu_legacy_bios.bin: qemu_legacy_start.elf
	$(OBJCOPY) -O binary $< $@
	truncate -s 262144 $@

qemu_legacy_start.elf: qemu_start.o qemu_stage1.o blob_service.o qemu_stage2_blob.o bios_blob.o platform/qemu/stage1/stage1.ld
	$(LD) -m elf_i386 -T platform/qemu/stage1/stage1.ld -Map qemu_legacy.map -o $@ qemu_start.o qemu_stage1.o blob_service.o qemu_stage2_blob.o bios_blob.o

qemu_flat_test_bios.bin: qemu_flat_test_start.elf
	$(OBJCOPY) -O binary $< $@
	truncate -s 262144 $@

qemu_flat_test_start.elf: qemu_start.o qemu_stage1.o blob_service.o qemu_stage2_blob.o bios_blob.o test_elf_blob.o platform/qemu/stage1/stage1.ld
	$(LD) -m elf_i386 -T platform/qemu/stage1/stage1.ld -Map qemu_flat_test.map -o $@ qemu_start.o qemu_stage1.o blob_service.o qemu_stage2_blob.o bios_blob.o test_elf_blob.o

qemu_legacy_test_bios.bin: qemu_legacy_bios.bin test_floppy_blob.bin $(BUILD_SCRIPTS)/patch_rom_free.py
	cp qemu_legacy_bios.bin $@
	python3 $(BUILD_SCRIPTS)/patch_rom_free.py $@ test_floppy_blob.bin

qemu_start.o: platform/qemu/stage1/qemu_start.asm
	nasm $(NASMFLAGS) -f elf32 $< -o $@

qemu_stage1.o: platform/qemu/stage1/qemu_stage1.c
	$(COMPILE_C)
