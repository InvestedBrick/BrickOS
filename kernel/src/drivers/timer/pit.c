#include "../../io/io.h"

#include "pit.h"
uint64_t timer_freq;

void init_pit(uint32_t desired_freq) {
    uint32_t reload = PIT_FREQUENCY / desired_freq;
    // https://forum.osdev.org/viewtopic.php?p=315727
    timer_freq = ((uint64_t)3579545 << 32) / (reload * 3);
    outb(PIT_COMMAND,PIT_SETTINGS);
    outb(PIT_CHANNEL0, (uint8_t)(reload & 0xff));
    outb(PIT_CHANNEL0, (uint8_t)((reload >> 8) & 0xff));
}