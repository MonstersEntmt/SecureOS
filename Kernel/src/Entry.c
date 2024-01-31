#include "Entry.h"
#include "Memory/PMM.h"
#include "Memory/VMM.h"
#include "Panic.h"

#include <stdio.h>
#include <string.h>

void KernelSelectAllocatorsFromCommandLine(const char* commandLine)
{
	size_t commandLineLen = strlen(commandLine);
	size_t commandBegin   = 0;
	while (commandBegin < commandLineLen)
	{
		size_t commandEnd = (const char*) memchr(commandLine + commandBegin, ' ', commandLineLen - commandBegin) - commandLine;
		size_t commandLen = commandEnd - commandBegin;
		if (commandLen > 5)
		{
			if (memcmp(commandLine + commandBegin, "-pmm=", 5) == 0)
				PMMSelect(commandLine + commandBegin + 5, commandLen - 5);
			else if (memcmp(commandLine + commandBegin, "-vmm=", 5) == 0)
				VMMSelect(commandLine + commandBegin + 5, commandLen - 5);
		}
		commandBegin = commandEnd + 1;
	}
}

void KernelEntry(void)
{
	puts("INFO: Kernel entry reached");
	KernelPanic();
}