# Included from ../../../Makefile; paths are relative to src/.

QEMU_STAGE1_C_OBJS := platform/qemu/stage1/qemu_stage1.o
QEMU_STAGE1_OBJS := platform/qemu/stage1/qemu_start.o \
	$(QEMU_STAGE1_C_OBJS) $(SHARED_SERVICE_OBJS)

$(QEMU_STAGE1_ELF): $(QEMU_STAGE1_OBJS) $(SHARED_SERVICE_RELOC_CHECK) platform/qemu/stage1/stage1.ld
	$(LD) -m elf_i386 -T platform/qemu/stage1/stage1.ld -Map platform/qemu/stage1/stage1.map -o $@ $(QEMU_STAGE1_OBJS)

qemu_bios.bin: qemu_linux_genrom.bin
	cp $< $@

qemu_legacy_bios.bin: qemu_legacy_genrom.bin
	cp $< $@

qemu_legacy_test_bios.bin: qemu_legacy_bios.bin test_floppy_blob.bin $(BUILD_SCRIPTS)/patch_rom_free.py
	cp qemu_legacy_bios.bin $@
	python3 $(BUILD_SCRIPTS)/patch_rom_free.py $@ test_floppy_blob.bin

platform/qemu/stage1/qemu_start.o: platform/qemu/stage1/qemu_start.asm | include/blob.inc shared_service/service_table.inc
	$(COMPILE_ASM_ELF32)

platform/qemu/stage1/qemu_stage1.o: platform/qemu/stage1/qemu_stage1.c
	$(COMPILE_C)
