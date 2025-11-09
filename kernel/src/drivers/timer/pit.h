
#ifndef INCLUDE_PIT_H
#define INCLUDE_PIT_H


#define PIT_CHANNEL0 0x40
#define PIT_COMMAND 0x43
/**
 * Channel 0
 * lobyte/hibyte
 * square wave gen
 * 16 bit binary mode
 */
#define PIT_SETTINGS 0x36 
#define PIT_FREQUENCY 1193182
// desire a PIT frequency of around 1 kHz
#define DESIRED_STANDARD_FREQ 1000

/**
 * init_pit:
 * Initializes the Programmable Interval Timer
 * @param desired_freq The desired frequency
 */
void init_pit(uint32_t desired_freq);

#endif