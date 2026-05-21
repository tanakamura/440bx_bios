# Included from ../../Makefile; paths are relative to src/.

P2B98_XV_GENROM_BINS := p2b98_xv_legacy_genrom.bin \
	p2b98_xv_selftest_genrom.bin p2b98_xv_linux_genrom.bin
P2B98_XV_BLOB_DEPFILES := platform/p2b98_xv/legacy.blobs.d \
	platform/p2b98_xv/selftest.blobs.d platform/p2b98_xv/linux.blobs.d

platform/p2b98_xv/%.blobs.d: platform/p2b98_xv/%.blobs $(BUILD_SCRIPTS)/gen_rom_deps.py $(BUILD_SCRIPTS)/gen_rom.py
	python3 $(BUILD_SCRIPTS)/gen_rom_deps.py --target p2b98_xv_$*_genrom.bin $< $@

ifneq ($(filter clean,$(MAKECMDGOALS)),clean)
-include $(P2B98_XV_BLOB_DEPFILES)
endif

p2b98_xv_%_genrom.bin: platform/p2b98_xv/%.blobs $(BUILD_SCRIPTS)/gen_rom.py $(BUILD_SCRIPTS)/gen_blob.py
	python3 $(BUILD_SCRIPTS)/gen_rom.py $< $@
