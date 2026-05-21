# Included from ../Makefile; paths are relative to src/.

testfd.img: $(BUILD_SCRIPTS)/gen_testfd.py
	python3 $(BUILD_SCRIPTS)/gen_testfd.py

int13probe.bin: tests/boot/int13probe.asm
	$(COMPILE_ASM_BIN)

int13probe.img: int13probe.bin $(BUILD_SCRIPTS)/gen_int13probe.py
	python3 $(BUILD_SCRIPTS)/gen_int13probe.py

usbmbr.bin: tests/boot/usbmbr.asm
	$(COMPILE_ASM_BIN)

usbmbr.img: usbmbr.bin
	cp $< $@
	truncate -s 1474560 $@

linuxprobe.o: tests/boot/linuxprobe.asm
	$(COMPILE_ASM_ELF32)

linuxprobe.elf: linuxprobe.o tests/boot/linuxprobe.ld
	$(LD) -m elf_i386 -T tests/boot/linuxprobe.ld -o $@ linuxprobe.o

linuxprobe.img linuxprobe_raw.img: linuxprobe.elf $(BUILD_SCRIPTS)/gen_linuxprobe_img.py
	python3 $(BUILD_SCRIPTS)/gen_linuxprobe_img.py

linuxprobe_overlap.o: tests/boot/linuxprobe_overlap.asm
	$(COMPILE_ASM_ELF32)

linuxprobe_overlap.elf: linuxprobe_overlap.o tests/boot/linuxprobe_overlap.ld
	$(LD) -m elf_i386 -T tests/boot/linuxprobe_overlap.ld -o $@ linuxprobe_overlap.o

linuxprobe_overlap.img linuxprobe_overlap_raw.img: linuxprobe_overlap.elf $(BUILD_SCRIPTS)/gen_linuxprobe_img.py
	python3 $(BUILD_SCRIPTS)/gen_linuxprobe_img.py linuxprobe_overlap.elf linuxprobe_overlap.img linuxprobe_overlap_raw.img

flashtest.img: flashutil.exe $(BUILD_SCRIPTS)/gen_flashtest_img.py
	python3 $(BUILD_SCRIPTS)/gen_flashtest_img.py

stdtest.img custbio.img custflat.img: biostest.exe flattest.exe shutdown.exe flashutil.exe $(BUILD_SCRIPTS)/gen_test_images.py
	python3 $(BUILD_SCRIPTS)/gen_test_images.py

test10.img: $(BUILD_SCRIPTS)/gen_test10.py
	python3 $(BUILD_SCRIPTS)/gen_test10.py

test_floppy_blob.bin: testfd.img $(BUILD_SCRIPTS)/gen_fdos_sparse.py
	python3 $(BUILD_SCRIPTS)/gen_fdos_sparse.py testfd.img test_floppy_blob.bin

romtest: tests/board/romtest.asm
	$(COMPILE_ASM_BIN)

test: stdtest.img usbmbr.img linuxprobe.img linuxprobe_raw.img linuxprobe_overlap.img qemu_legacy_test_bios.bin qemu_legacy_genrom.bin qemu_linux_genrom.bin
	python3 $(TEST_SCRIPTS)/run_qemu_tests.py
