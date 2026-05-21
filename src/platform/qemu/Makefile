# Included from ../../Makefile; paths are relative to src/.

QEMU_GENROM_BINS := qemu_legacy_genrom.bin qemu_selftest_genrom.bin \
	qemu_linux_genrom.bin

qemu_%_genrom.bin: platform/qemu/%.blobs $(GENROM_STAGE_ELFS) dsdt.aml ../images/gbsmc.088 $(BUILD_SCRIPTS)/gen_rom.py $(BUILD_SCRIPTS)/gen_blob.py
	python3 $(BUILD_SCRIPTS)/gen_rom.py $< $@

qemu_legacy_genrom.bin: legacy_app.elf
qemu_linux_genrom.bin: linux_loader_app.elf
