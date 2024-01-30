#pragma once

void              CPUDisableInterrupts(void);
void              CPUEnableInterrupts(void);
[[noreturn]] void CPUHalt(void);
[[noreturn]] void KernelPanic(void);