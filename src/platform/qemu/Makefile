# Included from ../../Makefile; paths are relative to src/.

QEMU_GENROM_BINS := qemu_legacy_genrom.bin qemu_selftest_genrom.bin \
	qemu_linux_genrom.bin
QEMU_BLOB_DEPFILES := platform/qemu/legacy.blobs.d \
	platform/qemu/selftest.blobs.d platform/qemu/linux.blobs.d

platform/qemu/%.blobs.d: platform/qemu/%.blobs $(BUILD_SCRIPTS)/gen_rom_deps.py $(BUILD_SCRIPTS)/gen_rom.py shared_service/service_table.h
	python3 $(BUILD_SCRIPTS)/gen_rom_deps.py --target qemu_$*_genrom.bin $< $@

ifneq ($(filter clean,$(MAKECMDGOALS)),clean)
-include $(QEMU_BLOB_DEPFILES)
endif

qemu_%_genrom.bin: platform/qemu/%.blobs $(BUILD_SCRIPTS)/gen_rom.py $(BUILD_SCRIPTS)/gen_blob.py shared_service/service_table.h
	python3 $(BUILD_SCRIPTS)/gen_rom.py $< $@
