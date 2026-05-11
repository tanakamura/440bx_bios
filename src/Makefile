CC := gcc
LD := ld
OBJCOPY := objcopy
CFLAGS := -O2 -m32 -march=pentium2 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fstack-usage -Wstack-usage=256 -nostdlib -nostartfiles -nodefaultlibs -Wall -Wextra
OBJCOPY_ELF_FLAGS := -I binary -O elf32-i386 -B i386

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

testfd.img: gen_testfd.py
	python3 gen_testfd.py

int13probe.bin: int13probe.asm
	nasm -f bin $< -o $@

int13probe.img: int13probe.bin gen_int13probe.py
	python3 gen_int13probe.py

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

start: start.elf
	$(OBJCOPY) -O binary --pad-to 0x100000 --gap-fill 0x90 $< $@

start.elf: start_asm.o bootblock.o blob_service.o l2_service.o bios_blob.o fdos_blob.o start.ld
	$(LD) -m elf_i386 -T start.ld -Map start.map -o $@ start_asm.o bootblock.o blob_service.o l2_service.o bios_blob.o fdos_blob.o

bios.elf: bios16.o bios_main.o bios.ld
	$(LD) -m elf_i386 -T bios.ld -Map bios.map -o $@ bios16.o bios_main.o

qemu_bios.bin: qemu_start.elf
	$(OBJCOPY) -O binary $< $@
	truncate -s 262144 $@

qemu_start.elf: qemu_start.o blob_service.o bios_blob.o fdos_flat_test_blob.o qemu.ld
	$(LD) -m elf_i386 -T qemu.ld -Map qemu.map -o $@ qemu_start.o blob_service.o bios_blob.o fdos_flat_test_blob.o

qemu_flat_test_bios.bin: qemu_flat_test_start.elf
	$(OBJCOPY) -O binary $< $@
	truncate -s 262144 $@

qemu_flat_test_start.elf: qemu_start.o blob_service.o bios_blob.o fdos_flat_test_blob.o qemu.ld
	$(LD) -m elf_i386 -T qemu.ld -Map qemu_flat_test.map -o $@ qemu_start.o blob_service.o bios_blob.o fdos_flat_test_blob.o

start_asm.o: start.asm post_code.inc
	nasm -f elf32 $< -o $@

bootblock.o: bootblock.c post_code.h blob.h l2_service.h
	$(CC) $(CFLAGS) -c $< -o $@

blob_service.o: blob_service.c blob.h
	$(CC) $(CFLAGS) -c $< -o $@

l2_service.o: l2_service.c l2_service.h
	$(CC) $(CFLAGS) -c $< -o $@

bios16.o: bios16.asm post_code.inc
	nasm -f elf32 $< -o $@

qemu_start.o: qemu_start.asm
	nasm -f elf32 $< -o $@

bios_main.o: bios_main.c post_code.h blob.h
	$(CC) $(CFLAGS) -c $< -o $@

romtest: romtest.asm
	nasm -f bin $< -o $@

test: stdtest.img custflat.img qemu_flat_test_bios.bin
	python3 run_qemu_tests.py

clean:
	rm -f start start.elf *.map *.o *.su *.bin *.exe *.com *.obj *.img romtest post_code.h post_code.inc

install-all: install-xrecv-exe install-mem-dmp-exe install-flashutil-exe
