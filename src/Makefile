CC := gcc
LD := ld
OBJCOPY := objcopy
CFLAGS := -O2 -m32 -march=pentium2 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fstack-usage -Wstack-usage=256 -nostdlib -nostartfiles -nodefaultlibs -Wall -Wextra

post_code.h post_code.inc ../specs/POST_CODE.md: post_code.def gen_post_codes.py
	python3 gen_post_codes.py

start: start.elf
	$(OBJCOPY) -O binary --pad-to 0x100000 --gap-fill 0x90 $< $@

start.elf: start_asm.o main.o start.ld
	$(LD) -m elf_i386 -T start.ld -Map start.map -o $@ start_asm.o main.o

start_asm.o: start.asm post_code.inc
	nasm -f elf32 $< -o $@

main.o: main.c post_code.h
	$(CC) $(CFLAGS) -c $< -o $@

romtest: romtest.asm
	nasm -f bin $< -o $@

clean:
	rm -f start start.elf start.map start_asm.o main.o main.su romtest post_code.h post_code.inc
