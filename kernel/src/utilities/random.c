#include "random.h"
#include "../kernel_header.h"

static uint64_t random_state;

void simple_rand_init(uint64_t seed)
{
    if (seed == 0)
        seed = 0x9E3779B97F4A7C15ULL;

    random_state = seed;
}

uint64_t simple_rand_u64(void)
{
    uint64_t x = random_state;

    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;

    random_state = x;

    return x * 0x2545F4914F6CDD1DULL;
}
#include "../io/log.h"
void setup_rng(){
    uint64_t seed = limine_data.boot_time ^ (uint64_t)(&seed) ^ (limine_data.rsdp >> 16);
    simple_rand_init(limine_data.boot_time);

}