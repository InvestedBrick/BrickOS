TARGET = interrupt
ASM = interrupt.asm
all: $(TARGET)

$(TARGET): $(ASM)
	nasm -f bin $(ASM) -o $(TARGET)

clean:
	rm interrupt