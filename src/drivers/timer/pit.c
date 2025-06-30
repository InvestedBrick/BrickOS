#include "../../io/io.h"

#include "pit.h"

void init_pit(unsigned int desired_freq) {
    unsigned int divisor = PIT_FREQUENCY / desired_freq;
    outb(PIT_COMMAND,PIT_SETTINGS);
    outb(PIT_CHANNEL0, (unsigned char)(divisor & 0xff));
    outb(PIT_CHANNEL0, (unsigned char)((divisor >> 8) & 0xff));
}