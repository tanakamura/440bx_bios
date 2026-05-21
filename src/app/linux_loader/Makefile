# Included from ../../Makefile; paths are relative to src/.

LINUX_LOADER_OBJS := linux_loader.o
LINUX_LOADER_APP_OBJS := linux_loader_app_entry.o $(LINUX_LOADER_OBJS)
LINUX_LOADER_C_OBJS := $(LINUX_LOADER_APP_OBJS)

$(LINUX_LOADER_APP_ELF): $(LINUX_LOADER_APP_OBJS) app/linux_loader/linux_loader.ld
	$(LD) -m elf_i386 -T app/linux_loader/linux_loader.ld -Map app/linux_loader/linux_loader_app.map -o $@ $(LINUX_LOADER_APP_OBJS)

app/linux_loader/linux_loader_app.bin: $(LINUX_LOADER_APP_ELF)
	$(OBJCOPY) -O binary $< $@

linux_loader.o: app/linux_loader/linux_loader.c
	$(COMPILE_C)

linux_loader_app_entry.o: app/linux_loader/entry.c
	$(COMPILE_C)
