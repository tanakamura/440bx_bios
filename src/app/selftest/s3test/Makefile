# Included from ../../../Makefile; paths are relative to src/.

UACPI_CPPFLAGS := $(CPPFLAGS) -Iapp/selftest/s3test -I../uACPI/include \
	-DUACPI_OVERRIDE_CONFIG
UACPI_CFLAGS := $(CFLAGS) -Os -ffunction-sections -fdata-sections \
	-Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers
UACPI_SRC_NAMES := tables types uacpi utilities interpreter opcodes namespace \
	stdlib shareable opregion default_handlers io notify sleep registers \
	resources event mutex osi
UACPI_OBJS := $(addsuffix .o,$(addprefix app/selftest/s3test/uacpi_,$(UACPI_SRC_NAMES)))
SELFTEST_LOADER_OBJS := app/selftest/s3test/selftest_app.o \
	lib/x86/bios_memory.o lib/x86/bios_nvram.o lib/x86/bios_rtc.o \
	lib/serial/bios_serial.o lib/acpi/bios_acpi_runtime.o \
	lib/pci/bios_pci.o
S3TEST_EXEC_OBJS := app/selftest/s3test/s3test.o \
	app/selftest/s3test/s3test_uacpi.o $(UACPI_OBJS)
SELFTEST_C_OBJS := $(SELFTEST_LOADER_OBJS) $(S3TEST_EXEC_OBJS)

app/selftest/s3test/s3test_uacpi.o: app/selftest/s3test/s3test_uacpi.c
	$(COMPILE_UACPI_C)

app/selftest/s3test/uacpi_%.o: ../uACPI/source/%.c
	$(COMPILE_UACPI_C)

$(SELFTEST_ELF): $(SELFTEST_LOADER_OBJS) app/selftest/s3test/selftest_app.ld include/blob.ld
	$(LD) -m elf_i386 --gc-sections -T app/selftest/s3test/selftest_app.ld -Map app/selftest/s3test/selftest_app.map -o $@ $(SELFTEST_LOADER_OBJS) $(LIBGCC)

S3TEST_EXEC_ELF := app/selftest/s3test/s3test_exec.elf
S3TEST_EXEC_BIN := app/selftest/s3test/s3test_exec.bin
S3TEST_EXEC_BLOB := app/selftest/s3test/s3test_exec_blob.bin

$(S3TEST_EXEC_ELF): $(S3TEST_EXEC_OBJS) app/selftest/s3test/s3test.ld
	$(LD) -m elf_i386 --gc-sections -T app/selftest/s3test/s3test.ld -Map app/selftest/s3test/s3test_exec.map -o $@ $(S3TEST_EXEC_OBJS) $(LIBGCC)

$(S3TEST_EXEC_BIN): $(S3TEST_EXEC_ELF)
	$(OBJCOPY) -O binary $< $@

$(S3TEST_EXEC_BLOB): $(S3TEST_EXEC_BIN) $(BUILD_SCRIPTS)/gen_selftest_exec_blob.py include/selftest_exec_blob.h
	python3 $(BUILD_SCRIPTS)/gen_selftest_exec_blob.py $< 0x00180000 $@
