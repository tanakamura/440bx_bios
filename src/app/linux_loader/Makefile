# Included from ../../Makefile; paths are relative to src/.

LINUX_LOADER_OBJS := app/linux_loader/linux_loader.o \
	app/linux_loader/linux_rm_service_stub.o
LINUX_LOADER_APP_LIB_OBJS := lib/rm_thunk/bios16.o \
	lib/acpi/acpi_tables.o lib/acpi/bios_acpi_runtime.o \
	lib/serial/bios_serial.o lib/pci/bios_pci.o lib/storage/bios_storage.o \
	lib/x86/bios_memory.o lib/x86/bios_nvram.o lib/x86/bios_rtc.o
LINUX_LOADER_APP_OBJS := app/linux_loader/entry.o $(LINUX_LOADER_OBJS) \
	$(LINUX_LOADER_APP_LIB_OBJS)
LINUX_LOADER_C_OBJS := $(LINUX_LOADER_APP_OBJS)

$(LINUX_LOADER_APP_ELF): $(LINUX_LOADER_APP_OBJS) app/linux_loader/linux_loader.ld include/blob.ld
	$(LD) -m elf_i386 -T app/linux_loader/linux_loader.ld -Map app/linux_loader/linux_loader_app.map -o $@ $(LINUX_LOADER_APP_OBJS)

app/linux_loader/linux_loader_app.bin: $(LINUX_LOADER_APP_ELF)
	$(OBJCOPY) -O binary $< $@
