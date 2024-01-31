#pragma once

void KernelArchPreInit(void);
void KernelArchPostInit(void);
void KernelSelectAllocatorsFromCommandLine(const char* commandLine);
void KernelEntry(void);