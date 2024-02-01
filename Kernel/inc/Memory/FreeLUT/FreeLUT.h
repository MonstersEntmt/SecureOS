#pragma once

#include <stdint.h>

// Returns the smallest value for a specific LUT index
uint64_t FreeLUTGetValue(uint8_t index);
// Returns the index in the LUT where to push the value in
uint8_t FreeLUTGetFloorIndex(uint64_t value);
// Returns the index in the LUT where the value is at least what's requested
uint8_t FreeLUTGetCeilIndex(uint64_t value);