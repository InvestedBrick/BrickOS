#ifndef INCLUDE_RANDOM_H
#define INCLUDE_RANDOM_H
#include <stdint.h>

/**
 * setup_rng:
 * initializes all (pseudo) random generators based on boot data 
 */
void setup_rng();


/**
 * simple_rand_u64:
 * @return A pseudorandom 64 bit number
 */
uint64_t simple_rand_u64();
#endif