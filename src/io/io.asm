global outb
global inb
global outw
global inw
 ; outb - send a byte to an I/O port
 ; stack: [esp + 8] the data byte
 ;        [esp + 4] the I/O port
 ;        [esp    ] return address
outb:
    mov al, [esp + 8] ; data to be sent
    mov dx, [esp + 4] ; address of I/O port
    out dx, al
    ret

 ; inb - recieves byte from an I/O port
 ; stack: [esp + 4] the I/O port
 ;        [esp    ] return address
inb:
    mov dx, [esp + 4]
    in al, dx
    ret

outw:
    mov ax, [esp + 8]
    mov dx, [esp + 4]
    out dx, ax
    ret

inw:
    mov dx, [esp + 4]
    in ax, dx
    ret