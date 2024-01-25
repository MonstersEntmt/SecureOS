#include "Log.h"
#include "ACPI/ACPI.h"
#include "Build.h"
#include "DebugCon.h"
#include "Graphics/Graphics.h"
#include "KernelVMM.h"
#include "PMM.h"
#include "VMM.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct LogLine
{
	char*  Buffer;
	size_t Capacity;
	size_t Size;
};

struct LogState
{
	struct LogLine*    Lines;
	size_t             Capacity;
	size_t             Size;
	size_t             CurLine;
	size_t             CurColumn;
	struct Framebuffer Framebuffer;
};

uint8_t g_LogLockProcID = 0;
uint8_t g_LogLockCount  = 0;

struct LogState g_LogState = (struct LogState) {
	.Lines     = nullptr,
	.Capacity  = 0,
	.Size      = 0,
	.CurLine   = 0,
	.CurColumn = 0
};

void LogInit(struct Framebuffer* framebuffer)
{
	if (!framebuffer)
	{
		LogCritical("Log", "LogInit got a nullptr framebuffer!");
		return;
	}

	size_t maxLineSize       = framebuffer->Width / 10;
	size_t lineCount         = 4096 / sizeof(struct LogLine);
	size_t requiredPageCount = 1 + (lineCount * maxLineSize + 4095) / 4096;

	void* kernelPageTable = GetKernelPageTable();
	g_LogState.Lines      = VMMAlloc(kernelPageTable, requiredPageCount, 0, VMM_PAGE_TYPE_4KIB, VMM_PAGE_PROTECT_READ_WRITE);
	if (!g_LogState.Lines)
	{
		LogCritical("Log", "LogInit failed to allocate line buffers for log!");
		return;
	}
	for (size_t i = 0; i < requiredPageCount; ++i)
	{
		void* physicalPage = PMMAlloc(1);
		if (!physicalPage)
		{
			LogCritical("Log", "LogInit failed to allocate line buffers for log!");
			return;
		}
		memset(physicalPage, 0, 4096);
		VMMMap(kernelPageTable, (uint8_t*) g_LogState.Lines + i * 4096, physicalPage);
	}
	g_LogState.Capacity     = lineCount;
	g_LogState.Size         = 0;
	uint8_t* startOfBuffers = (uint8_t*) g_LogState.Lines + 4096;
	for (size_t i = 0; i < g_LogState.Capacity; ++i)
	{
		g_LogState.Lines[i] = (struct LogLine) {
			.Buffer   = (char*) (startOfBuffers + i * maxLineSize),
			.Capacity = maxLineSize,
			.Size     = 0
		};
	}
	g_LogState.Framebuffer = *framebuffer;
}

void LogLock(void)
{
	uint8_t curProcessorID    = GetProcessorID();
	uint8_t* restrict logLock = &g_LogLockProcID;
	if (*logLock == curProcessorID)
	{
		++g_LogLockCount;
		return;
	}
	while (*logLock != 0);
	*logLock = curProcessorID;
	++g_LogLockCount;
}

void LogUnlock(void)
{
	uint8_t curProcessorID = GetProcessorID();
	if (g_LogLockProcID != curProcessorID)
	{
		// TODO(MarcasRealAccount): Should we PANIC?
		return;
	}
	if (--g_LogLockCount == 0)
		g_LogLockProcID = 0;
}

static void LogFlush(void)
{
	struct GraphicsRect rect = (struct GraphicsRect) { .x = 0, .y = 0, .w = g_LogState.Framebuffer.Width, .h = g_LogState.Framebuffer.Height };
	struct LinearColor  bg   = (struct LinearColor) { .r = 0, .g = 0, .b = 0, .a = 65535 };
	struct LinearColor  fg   = (struct LinearColor) { .r = 65535, .g = 65535, .b = 65535, .a = 65535 };

	GraphicsDrawRect(&g_LogState.Framebuffer, rect, bg, bg);
	size_t maxLines  = g_LogState.Framebuffer.Height / 18;
	size_t startLine = 0;
	if (g_LogState.Size > maxLines)
		startLine = g_LogState.Size - maxLines;

	size_t curY = 0;
	for (size_t i = startLine; i < g_LogState.Size; ++i)
	{
		struct LogLine* line = &g_LogState.Lines[i];
		GraphicsDrawText(&g_LogState.Framebuffer, (struct GraphicsPoint) { .x = 0, .y = curY }, line->Buffer, line->Size, fg);
		curY += 18;
	}
}

static bool LogWriteChar(FILE* restrict file, char c)
{
#if BUILD_IS_CONFIG_DEBUG
	DebugCon_WriteChar(c);
#endif
	if (g_LogState.Lines && g_LogState.Capacity > 0)
	{
		bool shouldFlush = false;
		if (g_LogState.CurLine == g_LogState.Capacity)
		{
			struct LogLine tempLine = g_LogState.Lines[0];
			tempLine.Size           = 0;
			for (size_t i = 1; i < g_LogState.Capacity; ++i)
				g_LogState.Lines[i - 1] = g_LogState.Lines[i];
			g_LogState.Lines[g_LogState.Capacity - 1] = tempLine;
			--g_LogState.CurLine;
			shouldFlush = true;
		}

		switch (c)
		{
		case '\r': g_LogState.CurColumn = 0; break;
		case '\n':
			g_LogState.CurColumn = 0;
			++g_LogState.CurLine;
			shouldFlush = true;
			break;
		case '\0': break;
		default:
		{
			struct LogLine* line                 = &g_LogState.Lines[g_LogState.CurLine];
			line->Buffer[g_LogState.CurColumn++] = c;
			line->Size                           = g_LogState.CurColumn;
			if (g_LogState.CurColumn == line->Capacity)
			{
				g_LogState.CurColumn = 0;
				++g_LogState.CurLine;
				shouldFlush = true;
			}
			break;
		}
		}
		if (g_LogState.CurLine > g_LogState.Size)
			g_LogState.Size = g_LogState.CurLine;
		if (shouldFlush)
			LogFlush();
	}
	return true;
}

static size_t LogWriteChars(FILE* restrict file, const char* restrict str, size_t length)
{
#if BUILD_IS_CONFIG_DEBUG
	DebugCon_WriteChars(str, length);
#endif
	if (g_LogState.Lines && g_LogState.Capacity > 0)
	{
		bool shouldFlush = false;
		for (size_t i = 0; i < length; ++i)
		{
			char c = str[i];
			if (g_LogState.CurLine == g_LogState.Capacity)
			{
				struct LogLine tempLine = g_LogState.Lines[0];
				tempLine.Size           = 0;
				for (size_t i = 1; i < g_LogState.Capacity; ++i)
					g_LogState.Lines[i - 1] = g_LogState.Lines[i];
				g_LogState.Lines[g_LogState.Capacity - 1] = tempLine;
				--g_LogState.CurLine;
				shouldFlush = true;
			}

			switch (c)
			{
			case '\r': g_LogState.CurColumn = 0; break;
			case '\n':
				g_LogState.CurColumn = 0;
				++g_LogState.CurLine;
				shouldFlush = true;
				break;
			case '\0': break;
			default:
			{
				struct LogLine* line                 = &g_LogState.Lines[g_LogState.CurLine];
				line->Buffer[g_LogState.CurColumn++] = c;
				line->Size                           = g_LogState.CurColumn;
				if (g_LogState.CurColumn == line->Capacity)
				{
					g_LogState.CurColumn = 0;
					++g_LogState.CurLine;
					shouldFlush = true;
				}
				break;
			}
			}
		}
		if (g_LogState.CurLine > g_LogState.Size)
			g_LogState.Size = g_LogState.CurLine;
		if (shouldFlush)
			LogFlush();
	}
	return length;
}

void Log(enum LogSeverity severity, const char* id, const char* message)
{
#if !BUILD_IS_CONFIG_DEBUG
	if (severity == LogSeverityDebug)
		return;
#endif

	struct __FILE logStream;
	logStream.WriteChar  = LogWriteChar;
	logStream.WriteChars = LogWriteChars;

	uint8_t processorID = GetProcessorID();

	const char* severityStr = nullptr;
	switch (severity)
	{
	case LogSeverityInfo: severityStr = "Info"; break;
	case LogSeverityDebug: severityStr = "Debug"; break;
	case LogSeverityWarn: severityStr = "Warn"; break;
	case LogSeverityError: severityStr = "Error"; break;
	case LogSeverityCritical: severityStr = "Critical"; break;
	default: severityStr = "Unknown"; break;
	}

	LogLock();
	fprintf(&logStream, "%-8s %8.8s (%hhu): %s\n", severityStr, id, processorID, message);
	LogUnlock();
}

void LogInfo(const char* id, const char* message)
{
	Log(LogSeverityInfo, id, message);
}

void LogDebug(const char* id, const char* message)
{
#if BUILD_IS_CONFIG_DEBUG
	Log(LogSeverityDebug, id, message);
#endif
}

void LogWarn(const char* id, const char* message)
{
	Log(LogSeverityWarn, id, message);
}

void LogError(const char* id, const char* message)
{
	Log(LogSeverityError, id, message);
}

void LogCritical(const char* id, const char* message)
{
	Log(LogSeverityCritical, id, message);
}

void LogFormatted(enum LogSeverity severity, const char* id, const char* fmt, ...)
{
#if !BUILD_IS_CONFIG_DEBUG
	if (severity == LogSeverityDebug)
		return;
#endif

	struct __FILE logStream;
	logStream.WriteChar  = LogWriteChar;
	logStream.WriteChars = LogWriteChars;

	uint8_t processorID = GetProcessorID();

	const char* severityStr = nullptr;
	switch (severity)
	{
	case LogSeverityInfo: severityStr = "Info"; break;
	case LogSeverityDebug: severityStr = "Debug"; break;
	case LogSeverityWarn: severityStr = "Warn"; break;
	case LogSeverityError: severityStr = "Error"; break;
	case LogSeverityCritical: severityStr = "Critical"; break;
	default: severityStr = "Unknown"; break;
	}

	LogLock();
	va_list vlist;
	va_start(vlist, fmt);
	fprintf(&logStream, "%-8s %8.8s (%02hhX): %Ls\n", severityStr, id, processorID, fmt, &vlist);
	va_end(vlist);
	LogUnlock();
}

void LogInfoFormatted(const char* id, const char* fmt, ...)
{
	struct __FILE logStream;
	logStream.WriteChar  = LogWriteChar;
	logStream.WriteChars = LogWriteChars;

	uint8_t processorID = GetProcessorID();

	LogLock();
	va_list vlist;
	va_start(vlist, fmt);
	fprintf(&logStream, "Info     %8.8s (%02hhX): %Ls\n", id, processorID, fmt, &vlist);
	va_end(vlist);
	LogUnlock();
}

void LogDebugFormatted(const char* id, const char* fmt, ...)
{
#if BUILD_IS_CONFIG_DEBUG
	struct __FILE logStream;
	logStream.WriteChar  = LogWriteChar;
	logStream.WriteChars = LogWriteChars;

	uint8_t processorID = GetProcessorID();

	LogLock();
	va_list vlist;
	va_start(vlist, fmt);
	fprintf(&logStream, "Debug    %8.8s (%02hhX): %Ls\n", id, processorID, fmt, &vlist);
	va_end(vlist);
	LogUnlock();
#endif
}

void LogWarnFormatted(const char* id, const char* fmt, ...)
{
	struct __FILE logStream;
	logStream.WriteChar  = LogWriteChar;
	logStream.WriteChars = LogWriteChars;

	uint8_t processorID = GetProcessorID();

	LogLock();
	va_list vlist;
	va_start(vlist, fmt);
	fprintf(&logStream, "Warn     %8.8s (%02hhX): %Ls\n", id, processorID, fmt, &vlist);
	va_end(vlist);
	LogUnlock();
}

void LogErrorFormatted(const char* id, const char* fmt, ...)
{
	struct __FILE logStream;
	logStream.WriteChar  = LogWriteChar;
	logStream.WriteChars = LogWriteChars;

	uint8_t processorID = GetProcessorID();

	LogLock();
	va_list vlist;
	va_start(vlist, fmt);
	fprintf(&logStream, "Error    %8.8s (%02hhX): %Ls\n", id, processorID, fmt, &vlist);
	va_end(vlist);
	LogUnlock();
}

void LogCriticalFormatted(const char* id, const char* fmt, ...)
{
	struct __FILE logStream;
	logStream.WriteChar  = LogWriteChar;
	logStream.WriteChars = LogWriteChars;

	uint8_t processorID = GetProcessorID();

	LogLock();
	va_list vlist;
	va_start(vlist, fmt);
	fprintf(&logStream, "Critical %8.8s (%02hhX): %Ls\n", id, processorID, fmt, &vlist);
	va_end(vlist);
	LogUnlock();
}