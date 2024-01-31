#pragma once

#include "UltraProtocol.h"

#include <stdint.h>

void UltraEntry(struct ultra_boot_context* bootContext, uint32_t magic);
void UltraHandleAttribs(struct ultra_attribute_header* attributes, uint32_t attributeCount);