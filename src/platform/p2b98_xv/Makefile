# Included from ../../Makefile; paths are relative to src/.

P2B98_XV_GENROM_BINS := p2b98_xv_legacy_genrom.bin \
	p2b98_xv_selftest_genrom.bin p2b98_xv_linux_genrom.bin

p2b98_xv_%_genrom.bin: platform/p2b98_xv/%.blobs $(GENROM_STAGE_ELFS) dsdt.aml ../images/gbsmc.088 $(BUILD_SCRIPTS)/gen_rom.py $(BUILD_SCRIPTS)/gen_blob.py
	python3 $(BUILD_SCRIPTS)/gen_rom.py $< $@

p2b98_xv_legacy_genrom.bin: $(LEGACY_APP_ELF)
p2b98_xv_linux_genrom.bin: $(LINUX_LOADER_APP_ELF)
p2b98_xv_selftest_genrom.bin: $(SELFTEST_ELF)
