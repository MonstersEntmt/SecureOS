#pragma once

void  KernelArchPreInit(void);
void  KernelArchPostInit(void);
void  KernelSelectAllocatorsFromCommandLine(const char* commandLine);
void  KernelSetupVMM(void);
void* KernelGetVMM(void);
void  KernelEntry(void);