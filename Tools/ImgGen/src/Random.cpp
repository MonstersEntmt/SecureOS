#include "Random.hpp"

#include <random>

std::mt19937_64 s_RNG(std::random_device {}());

uint32_t RandomU32()
{
	return (uint32_t) s_RNG();
}

uint64_t RandomU64()
{
	return (uint64_t) s_RNG();
}