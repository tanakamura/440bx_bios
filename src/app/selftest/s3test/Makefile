# Included from ../../../Makefile; paths are relative to src/.

UACPI_CPPFLAGS := $(CPPFLAGS) -Iapp/selftest/s3test -I../uACPI/include \
	-DUACPI_OVERRIDE_CONFIG
UACPI_CFLAGS := $(CFLAGS) -Os -ffunction-sections -fdata-sections \
	-Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers
UACPI_SRC_NAMES := tables types uacpi utilities interpreter opcodes namespace \
	stdlib shareable opregion default_handlers io notify sleep registers \
	resources event mutex osi
UACPI_OBJS := $(addsuffix .o,$(addprefix uacpi_,$(UACPI_SRC_NAMES)))
SELFTEST_C_OBJS := s3test.o s3test_uacpi.o $(UACPI_OBJS)

s3test.o: app/selftest/s3test/s3test.c
	$(COMPILE_C)

s3test_uacpi.o: app/selftest/s3test/s3test_uacpi.c
	$(COMPILE_UACPI_C)

uacpi_%.o: ../uACPI/source/%.c
	$(COMPILE_UACPI_C)

s3test.elf: s3test.o s3test_uacpi.o $(UACPI_OBJS) app/selftest/s3test/s3test.ld
	$(LD) -m elf_i386 --gc-sections -T app/selftest/s3test/s3test.ld -Map s3test.map -o $@ s3test.o s3test_uacpi.o $(UACPI_OBJS) $(LIBGCC)
