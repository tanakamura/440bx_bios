# Included from ../../../Makefile; paths are relative to src/.

QEMU_STAGE1_C_OBJS := platform/qemu/stage1/qemu_stage1.o
QEMU_STAGE1_OBJS := platform/qemu/stage1/qemu_start.o \
	$(QEMU_STAGE1_C_OBJS) $(SHARED_SERVICE_OBJS)

$(QEMU_STAGE1_ELF): $(QEMU_STAGE1_OBJS) $(SHARED_SERVICE_RELOC_CHECK) platform/qemu/stage1/stage1.ld shared_service/service_table.ld
	$(LD) -m elf_i386 -T platform/qemu/stage1/stage1.ld -Map platform/qemu/stage1/stage1.map -o $@ $(QEMU_STAGE1_OBJS)

qemu_bios.bin: qemu_linux_genrom.bin
	cp $< $@

qemu_legacy_bios.bin: qemu_legacy_genrom.bin
	cp $< $@

qemu_legacy_test_bios.bin: qemu_legacy_bios.bin test_floppy_blob.bin $(BUILD_SCRIPTS)/patch_rom_free.py
	cp qemu_legacy_bios.bin $@
	python3 $(BUILD_SCRIPTS)/patch_rom_free.py $@ test_floppy_blob.bin

qemu_selftest_bios.bin: qemu_selftest_genrom.bin app/selftest/s3test/s3test_exec_blob.bin $(BUILD_SCRIPTS)/patch_rom_free.py
	cp qemu_selftest_genrom.bin $@
	python3 $(BUILD_SCRIPTS)/patch_rom_free.py $@ app/selftest/s3test/s3test_exec_blob.bin

qemu_benchmark_bios.bin: qemu_selftest_genrom.bin app/selftest/benchmark/benchmark_exec_blob.bin $(BUILD_SCRIPTS)/patch_rom_free.py
	cp qemu_selftest_genrom.bin $@
	python3 $(BUILD_SCRIPTS)/patch_rom_free.py $@ app/selftest/benchmark/benchmark_exec_blob.bin

platform/qemu/stage1/qemu_start.o: platform/qemu/stage1/qemu_start.asm | include/blob.inc shared_service/service_table.inc
	$(COMPILE_ASM_ELF32)
