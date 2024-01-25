#pragma once

#include "Graphics/Graphics.h"

enum LogSeverity
{
	LogSeverityInfo,
	LogSeverityDebug,
	LogSeverityWarn,
	LogSeverityError,
	LogSeverityCritical
};

void LogInit(struct Framebuffer* framebuffer);

void LogLock(void);
void LogUnlock(void);

void Log(enum LogSeverity severity, const char* id, const char* message);
void LogInfo(const char* id, const char* message);
void LogDebug(const char* id, const char* message);
void LogWarn(const char* id, const char* message);
void LogError(const char* id, const char* message);
void LogCritical(const char* id, const char* message);

void LogFormatted(enum LogSeverity severity, const char* id, const char* fmt, ...);
void LogInfoFormatted(const char* id, const char* fmt, ...);
void LogDebugFormatted(const char* id, const char* fmt, ...);
void LogWarnFormatted(const char* id, const char* fmt, ...);
void LogErrorFormatted(const char* id, const char* fmt, ...);
void LogCriticalFormatted(const char* id, const char* fmt, ...);