#pragma once

#include <cstdint>

uint32_t RandomU32();
uint64_t RandomU64();

inline int32_t RandomI32()
{
	return (int32_t) RandomU32();
}

inline int64_t RandomI64()
{
	return (int64_t) RandomU64();
}