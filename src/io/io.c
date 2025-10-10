#include "io.h"

void outb(uint16_t port, uint8_t data){
    asm volatile ("outb %0,%1" : : "a"(data), "Nd"(port));
}

void outw(uint16_t port, uint16_t data){
    asm volatile ("outw %0,%1" : : "a"(data), "Nd"(port));
}

uint8_t inb(uint16_t port){
    uint8_t ret;
    asm volatile ("inb %1,%0" : "=a"(ret) : "Nd"(port));
    return ret;
}
uint16_t inw(uint16_t port){
    uint16_t ret;
    asm volatile ("inw %1,%0" : "=a"(ret) : "Nd"(port));
    return ret;
}