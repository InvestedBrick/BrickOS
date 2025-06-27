
ASM_SRCS := $(wildcard *.asm)
BIN_OUTS := $(ASM_SRCS:.asm=.bin)

NASM := nasm
NASMFLAGS := -f bin

.PHONY: all clean

all: $(BIN_OUTS)

%.bin: %.asm
	$(NASM) $(NASMFLAGS) -o $@ $<

clean:
	rm -f $(BIN_OUTS)
