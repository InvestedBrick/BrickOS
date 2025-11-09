#include "../../io/io.h"

#include "pit.h"

void init_pit(uint32_t desired_freq) {
    uint32_t divisor = PIT_FREQUENCY / desired_freq;
    outb(PIT_COMMAND,PIT_SETTINGS);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xff));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xff));
}