CC := gcc
LD := ld
OBJCOPY := objcopy
LIBGCC := $(shell $(CC) -m32 -print-libgcc-file-name)
CPPFLAGS := -I.
CFLAGS := -O2 -m32 -march=pentium2 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fstack-usage -Wstack-usage=256 -nostdlib -nostartfiles -nodefaultlibs -Wall -Wextra
NASMFLAGS := -I.
OBJCOPY_ELF_FLAGS := -I binary -O elf32-i386 -B i386
UACPI_CPPFLAGS := $(CPPFLAGS) -I../uACPI/include -DUACPI_OVERRIDE_CONFIG
UACPI_CFLAGS := $(CFLAGS) -Os -ffunction-sections -fdata-sections \
	-Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers
UACPI_SRC_NAMES := tables types uacpi utilities interpreter opcodes namespace \
	stdlib shareable opregion default_handlers io notify sleep registers \
	resources event mutex osi
UACPI_OBJS := $(addsuffix .o,$(addprefix uacpi_,$(UACPI_SRC_NAMES)))

post_code.h post_code.inc ../specs/POST_CODE.md: post_code.def gen_post_codes.py
	python3 gen_post_codes.py

fdos_blob.bin: ../freedos_boot_fd.img gen_blob.py
	python3 gen_blob.py --fat12-normalize ../freedos_boot_fd.img fdos_blob.bin

fdos_flat_test_blob.bin: custflat.img gen_blob.py
	python3 gen_blob.py --fat12-normalize custflat.img fdos_flat_test_blob.bin

fdos_blob.o: fdos_blob.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.fdos,alloc,load,readonly,data,contents \
		$< $@

fdos_flat_test_blob.o: fdos_flat_test_blob.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.fdos,alloc,load,readonly,data,contents \
		$< $@

dsdt.aml: ../specs/dsdt.dsl gen_dsdt_aml.py
	python3 gen_dsdt_aml.py ../specs/dsdt.dsl dsdt.aml

dsdt_blob.bin: dsdt.aml gen_blob.py
	python3 gen_blob.py dsdt.aml dsdt_blob.bin

dsdt_blob.o: dsdt_blob.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.dsdtblob,alloc,load,readonly,data,contents \
		$< $@

vgabios_blob.bin: ../images/gbsmc.088 gen_blob.py
	python3 gen_blob.py ../images/gbsmc.088 vgabios_blob.bin

vgabios_blob.o: vgabios_blob.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.vgabiosblob,alloc,load,readonly,data,contents \
		$< $@

testfd.img: gen_testfd.py
	python3 gen_testfd.py

int13probe.bin: int13probe.asm
	nasm -f bin $< -o $@

int13probe.img: int13probe.bin gen_int13probe.py
	python3 gen_int13probe.py

usbmbr.bin: usbmbr.asm
	nasm -f bin $< -o $@

usbmbr.img: usbmbr.bin
	cp $< $@
	truncate -s 1474560 $@

linuxprobe.o: linuxprobe.asm
	nasm -f elf32 $< -o $@

linuxprobe.elf: linuxprobe.o linuxprobe.ld
	$(LD) -m elf_i386 -T linuxprobe.ld -o $@ linuxprobe.o

linuxprobe.img linuxprobe_raw.img: linuxprobe.elf gen_linuxprobe_img.py
	python3 gen_linuxprobe_img.py

s3test.o: s3test.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

s3test_uacpi.o: s3test_uacpi.c uacpi_config.h
	$(CC) $(UACPI_CPPFLAGS) $(UACPI_CFLAGS) -c $< -o $@

uacpi_%.o: ../uACPI/source/%.c uacpi_config.h
	$(CC) $(UACPI_CPPFLAGS) $(UACPI_CFLAGS) -c $< -o $@

s3test.elf: s3test.o s3test_uacpi.o $(UACPI_OBJS) s3test.ld
	$(LD) -m elf_i386 --gc-sections -T s3test.ld -Map s3test.map -o $@ s3test.o s3test_uacpi.o $(UACPI_OBJS) $(LIBGCC)

test_elf_blob.bin: s3test.elf gen_blob.py
	python3 gen_blob.py s3test.elf test_elf_blob.bin

test_elf_blob.o: test_elf_blob.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.testelfblob,alloc,load,readonly,data,contents \
		$< $@

srecsave.com: srecsave.asm
	nasm -f bin $< -o $@

install-srecsave: srecsave.com
	mcopy -o -i ../freedos_boot_fd.img srecsave.com ::SRECSAVE.COM

xrecv.exe: xrecv.c
	WATCOM=/opt/watcom INCLUDE=/opt/watcom/h PATH=/opt/watcom/binl64:$$PATH /opt/watcom/binl64/owcc -b dos -mcmodel=s -os -s $< -o $@

install-xrecv-exe: xrecv.exe
	mcopy -o -i ../freedos_boot_fd.img xrecv.exe ::XRECV.EXE

flashutil_io.obj: flashutil_io.asm
	nasm -f obj $< -o $@

flashutil.exe: flashutil.c flashutil_io.obj
	WATCOM=/opt/watcom INCLUDE=/opt/watcom/h PATH=/opt/watcom/binl64:$$PATH /opt/watcom/binl64/owcc -b dos -mcmodel=s -os -s flashutil.c flashutil_io.obj -o $@

install-flashutil-exe: flashutil.exe
	mcopy -o -i ../freedos_boot_fd.img flashutil.exe ::FLASHUTL.EXE

mem_dmp.exe: mem_dmp.c
	WATCOM=/opt/watcom INCLUDE=/opt/watcom/h PATH=/opt/watcom/binl64:$$PATH /opt/watcom/binl64/owcc -b dos -mcmodel=s -os -s $< -o $@

install-mem-dmp-exe: mem_dmp.exe
	mcopy -o -i ../freedos_boot_fd.img mem_dmp.exe ::MEM_DMP.EXE

flashtest.img: flashutil.exe gen_flashtest_img.py
	python3 gen_flashtest_img.py

biostest.exe: biostest.c
	WATCOM=/opt/watcom INCLUDE=/opt/watcom/h PATH=/opt/watcom/binl64:$$PATH /opt/watcom/binl64/owcc -b dos -mcmodel=s -os -s $< -o $@

flattest.exe: flattest.c
	WATCOM=/opt/watcom INCLUDE=/opt/watcom/h PATH=/opt/watcom/binl64:$$PATH /opt/watcom/binl64/owcc -b dos -mcmodel=s -os -s $< -o $@

shutdown.exe: shutdown.c
	WATCOM=/opt/watcom INCLUDE=/opt/watcom/h PATH=/opt/watcom/binl64:$$PATH /opt/watcom/binl64/owcc -b dos -mcmodel=s -os -s $< -o $@

stdtest.img custbio.img custflat.img: biostest.exe flattest.exe shutdown.exe flashutil.exe gen_test_images.py
	python3 gen_test_images.py

test10.img: gen_test10.py
	python3 gen_test10.py

test10_sparse.bin: test10.img gen_fdos_sparse.py
	python3 gen_fdos_sparse.py test10.img test10_sparse.bin

test10_sparse.o: test10_sparse.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.test10,alloc,load,readonly,data,contents \
		$< $@

testfd_sparse.bin: testfd.img gen_fdos_sparse.py
	python3 gen_fdos_sparse.py testfd.img testfd_sparse.bin

testfd_sparse.o: testfd_sparse.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.testfd,alloc,load,readonly,data,contents \
		$< $@

bios.bin: bios.elf
	$(OBJCOPY) -O binary $< $@

bios_blob.bin: bios.bin gen_blob.py
	python3 gen_blob.py bios.bin bios_blob.bin

bios_blob.o: bios_blob.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.biosblob,alloc,load,readonly,data,contents \
		$< $@

stage2.bin: stage2.elf
	$(OBJCOPY) -O binary $< $@

stage2_blob.bin: stage2.bin gen_blob.py
	python3 gen_blob.py stage2.bin stage2_blob.bin

stage2_blob.o: stage2_blob.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.stage2blob,alloc,load,readonly,data,contents \
		$< $@

qemu_stage2.bin: qemu_stage2.elf
	$(OBJCOPY) -O binary $< $@

qemu_stage2_blob.bin: qemu_stage2.bin gen_blob.py
	python3 gen_blob.py qemu_stage2.bin qemu_stage2_blob.bin

qemu_stage2_blob.o: qemu_stage2_blob.bin
	objcopy $(OBJCOPY_ELF_FLAGS) \
		--rename-section .data=.stage2blob,alloc,load,readonly,data,contents \
		$< $@

start: start.elf
	$(OBJCOPY) -O binary --pad-to 0x100000 --gap-fill 0x90 $< $@

start.elf: start_asm.o bootblock.o blob_service.o stage2_blob.o bios_blob.o dsdt_blob.o vgabios_blob.o test_elf_blob.o p2b98_xv/stage1/start.ld
	$(LD) -m elf_i386 -T p2b98_xv/stage1/start.ld -Map start.map -o $@ start_asm.o bootblock.o blob_service.o stage2_blob.o bios_blob.o dsdt_blob.o vgabios_blob.o test_elf_blob.o

bios.elf: bios16.o bios_main.o bios_serial.o bios_pci.o bios_storage.o bios.ld
	$(LD) -m elf_i386 -T bios.ld -Map bios.map -o $@ bios16.o bios_main.o bios_serial.o bios_pci.o bios_storage.o

stage2.elf: stage2.o l2_service.o p2b98_xv/stage2/stage2.ld
	$(LD) -m elf_i386 -T p2b98_xv/stage2/stage2.ld -Map stage2.map -o $@ stage2.o l2_service.o

qemu_bios.bin: qemu_start.elf
	$(OBJCOPY) -O binary $< $@
	truncate -s 262144 $@

qemu_start.elf: qemu_start.o blob_service.o qemu_stage2_blob.o bios_blob.o test_elf_blob.o qemu/stage1/qemu.ld
	$(LD) -m elf_i386 -T qemu/stage1/qemu.ld -Map qemu.map -o $@ qemu_start.o blob_service.o qemu_stage2_blob.o bios_blob.o test_elf_blob.o

qemu_flat_test_bios.bin: qemu_flat_test_start.elf
	$(OBJCOPY) -O binary $< $@
	truncate -s 262144 $@

qemu_flat_test_start.elf: qemu_start.o blob_service.o qemu_stage2_blob.o bios_blob.o test_elf_blob.o qemu/stage1/qemu.ld
	$(LD) -m elf_i386 -T qemu/stage1/qemu.ld -Map qemu_flat_test.map -o $@ qemu_start.o blob_service.o qemu_stage2_blob.o bios_blob.o test_elf_blob.o

start_asm.o: p2b98_xv/stage1/start.asm post_code.inc
	nasm $(NASMFLAGS) -f elf32 $< -o $@

bootblock.o: p2b98_xv/stage1/bootblock.c post_code.h blob.h bios_nvram.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

stage2.o: p2b98_xv/stage2/stage2.c blob.h p2b98_xv/stage2/l2_service.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

qemu_stage2.o: qemu/stage2/stage2.c blob.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

blob_service.o: blob_service.c blob.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

l2_service.o: p2b98_xv/stage2/l2_service.c p2b98_xv/stage2/l2_service.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

qemu_stage2.elf: qemu_stage2.o qemu/stage2/stage2.ld
	$(LD) -m elf_i386 -T qemu/stage2/stage2.ld -Map qemu_stage2.map -o $@ qemu_stage2.o

bios16.o: bios16.asm post_code.inc
	nasm $(NASMFLAGS) -f elf32 $< -o $@

qemu_start.o: qemu/stage1/qemu_start.asm
	nasm $(NASMFLAGS) -f elf32 $< -o $@

bios_main.o: bios_main.c post_code.h blob.h bios_io.h bios_nvram.h bios_pci.h bios_serial.h bios_storage.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

bios_serial.o: bios_serial.c bios_io.h bios_serial.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

bios_pci.o: bios_pci.c bios_io.h bios_pci.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

bios_storage.o: bios_storage.c post_code.h bios_io.h bios_pci.h bios_serial.h bios_storage.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

romtest: romtest.asm
	nasm -f bin $< -o $@

test: stdtest.img usbmbr.img linuxprobe.img linuxprobe_raw.img qemu_flat_test_bios.bin
	python3 run_qemu_tests.py

clean:
	rm -f start start.elf linuxprobe.elf *.map *.o *.su *.bin *.exe *.com *.obj *.img *.aml *.asl romtest post_code.h post_code.inc

install-all: install-xrecv-exe install-mem-dmp-exe install-flashutil-exe
