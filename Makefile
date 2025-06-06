ASM_SOURCES = $(wildcard src/*.asm)

LD_SCRIPT = link.ld
LD_ARGS = -m elf_i386

ASM_OBJS = $(ASM_SOURCES:.asm=.o)

TARGET = kernel.elf
ISO = BrickOS.iso

BRICK_ASM = src/kernel.asm
BRICK_OBJ = kernel.o

all: $(TARGET)

%.o: %.asm
	nasm -f elf32 $< -o $@

$(BRICK_ASM): src/kernel.brick
	brick -O2 -S src/kernel.brick 

$(BRICK_OBJ): $(BRICK_ASM)
	nasm -f elf32 $(BRICK_ASM) -o $(BRICK_OBJ)

$(TARGET): $(ASM_OBJS) $(BRICK_OBJ)
	ld $(LD_ARGS) -T $(LD_SCRIPT) $(ASM_OBJS) $(BRICK_OBJ) -o $(TARGET)


iso: $(TARGET)
	cp kernel.elf iso/boot
	bash stage3.sh

clean:
	rm -f $(ASM_OBJS) $(TARGET) $(ISO) $(BRICK_ASM) $(BRICK_OBJ)
