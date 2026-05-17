ZIG ?= zig
TARGET ?= x86-linux-musl
OPTIMIZE ?= ReleaseSmall

all: init

init: init.zig
	$(ZIG) build-exe $< -target $(TARGET) -O $(OPTIMIZE) -fstrip -fsingle-threaded -lc -femit-bin=$@

clean:
	rm -f init

.PHONY: all clean
