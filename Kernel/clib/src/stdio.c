#include "stdio.h"
#include "stdint.h"
#include "string.h"

size_t fscanf(FILE* restrict stream, const char* restrict fmt, ...)
{
	va_list vlist;
	va_start(vlist, fmt);
	size_t res = vfscanf(stream, fmt, vlist);
	va_end(vlist);
	return res;
}

size_t snscanf(const char* restrict buffer, size_t bufsize, const char* restrict fmt, ...)
{
	va_list vlist;
	va_start(vlist, fmt);
	size_t res = vsnscanf(buffer, bufsize, fmt, vlist);
	va_end(vlist);
	return res;
}

size_t fprintf(FILE* restrict stream, const char* restrict fmt, ...)
{
	va_list vlist;
	va_start(vlist, fmt);
	size_t res = vfprintf(stream, fmt, vlist);
	va_end(vlist);
	return res;
}

size_t snprintf(char* restrict buffer, size_t bufsize, const char* restrict fmt, ...)
{
	va_list vlist;
	va_start(vlist, fmt);
	size_t res = vsnprintf(buffer, bufsize, fmt, vlist);
	va_end(vlist);
	return res;
}

struct ReadBufferStream
{
	struct __FILE handles;

	const char* restrict buffer;
	size_t head;
	size_t size;
};

struct WriteBufferStream
{
	struct __FILE handles;

	char* restrict buffer;
	size_t head;
	size_t size;
};

static int BufferStreamReadChar(FILE* restrict file)
{
	struct ReadBufferStream* stream = (struct ReadBufferStream*) file;
	return stream->head < stream->size ? stream->buffer[stream->head++] : -1;
}

static size_t BufferStreamReadChars(FILE* restrict file, char* restrict buffer, size_t bufsize)
{
	struct ReadBufferStream* stream = (struct ReadBufferStream*) file;

	size_t rem    = stream->size - stream->head;
	size_t toCopy = bufsize < rem ? bufsize : rem;
	memcpy(buffer, stream->buffer + stream->head, toCopy);
	stream->head += toCopy;
	return toCopy;
}

static bool BufferStreamWriteChar(FILE* restrict file, char c)
{
	struct WriteBufferStream* stream = (struct WriteBufferStream*) file;
	if (stream->head >= stream->size)
		return false;
	stream->buffer[stream->head++] = c;
	return true;
}

static size_t BufferStreamWriteChars(FILE* restrict file, const char* restrict buffer, size_t bufsize)
{
	struct WriteBufferStream* stream = (struct WriteBufferStream*) file;

	size_t rem    = stream->size - stream->head;
	size_t toCopy = bufsize < rem ? bufsize : rem;
	memcpy(stream->buffer + stream->head, buffer, toCopy);
	stream->head += toCopy;
	return toCopy;
}

size_t vsnscanf(const char* restrict buffer, size_t bufsize, const char* restrict fmt, va_list vlist)
{
	if (!buffer || !bufsize)
		return vfscanf(nullptr, fmt, vlist);
	struct ReadBufferStream stream;
	stream.handles.ReadChar  = BufferStreamReadChar;
	stream.handles.ReadChars = BufferStreamReadChars;
	stream.buffer            = buffer;
	stream.head              = 0;
	stream.size              = bufsize;
	return vfscanf(&stream.handles, fmt, vlist);
}

size_t vsnprintf(char* restrict buffer, size_t bufsize, const char* restrict fmt, va_list vlist)
{
	if (!buffer || !bufsize)
		return vfprintf(nullptr, fmt, vlist);
	struct WriteBufferStream stream;
	stream.handles.WriteChar  = BufferStreamWriteChar;
	stream.handles.WriteChars = BufferStreamWriteChars;
	stream.buffer             = buffer;
	stream.head               = 0;
	stream.size               = bufsize;
	return vfprintf(&stream.handles, fmt, vlist);
}

static size_t StrFindFirst(const char* restrict str, char c, size_t offset)
{
	while (1)
	{
		char sc = str[offset];
		if (sc == '\0' || sc == c)
			return offset;
		++offset;
	}
}

struct FormatOptions
{
	bool     leftJustified;
	uint8_t  signMode;
	bool     altMode;
	bool     zeroPad;
	uint32_t fieldWidth;
	int32_t  precision;
	uint8_t  lengthMode;
	char     formatChar;
};

static inline size_t ParseFormatOptions(const char* restrict fmt, size_t offset, struct FormatOptions* options, va_list* vlist)
{
	options->leftJustified = false;
	options->signMode      = 0;
	options->altMode       = false;
	options->zeroPad       = false;
	options->fieldWidth    = 0;
	options->precision     = -1;
	options->lengthMode    = 2;
	options->formatChar    = '\0';

	// Parse primary options
	char c = fmt[offset];
	while (1)
	{
		switch (c)
		{
		case '-':
			options->leftJustified = true;
			c                      = fmt[++offset];
			break;
		case '+':
			options->signMode = 1;
			c                 = fmt[++offset];
			break;
		case ' ':
			options->signMode = 2;
			c                 = fmt[++offset];
			break;
		case '#':
			options->altMode = true;
			c                = fmt[++offset];
			break;
		case '0':
			options->zeroPad = true;
			c                = fmt[++offset];
			break;
		default:
			goto PRIMARY_BREAKOUT;
		}
		continue;
	PRIMARY_BREAKOUT:
		break;
	}

	// Parse field width
	while (1)
	{
		switch (c)
		{
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			if (options->fieldWidth < 0)
				options->fieldWidth = 0;
			else
				options->fieldWidth *= 10;
			options->fieldWidth += c - '0';
			c                    = fmt[++offset];
			break;
		case '*':
			options->fieldWidth = (uint32_t) va_arg(*vlist, int);
			c                   = fmt[++offset];
			break;
		default:
			goto FIELD_WIDTH_BREAKOUT;
		}
		continue;
	FIELD_WIDTH_BREAKOUT:
		break;
	}

	if (c == '.')
	{
		// Parse precision
		options->precision = 0;
		c                  = fmt[++offset];
		while (1)
		{
			switch (c)
			{
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				options->precision *= 10;
				options->precision += c - '0';
				c                   = fmt[++offset];
				break;
			case '*':
				options->precision = va_arg(*vlist, int);
				c                  = fmt[++offset];
				break;
			default:
				goto PRECISION_BREAKOUT;
			}
			continue;
		PRECISION_BREAKOUT:
			break;
		}
	}

	// Parse length
	switch (c)
	{
	case 'h':
		c = fmt[++offset];
		switch (c)
		{
		case 'h':
			options->lengthMode = 0;
			c                   = fmt[++offset];
			break;
		default:
			options->lengthMode = 1;
			break;
		}
		break;
	case 'l':
		c = fmt[++offset];
		switch (c)
		{
		case 'l':
			options->lengthMode = 4;
			c                   = fmt[++offset];
			break;
		default:
			options->lengthMode = 3;
			break;
		}
		break;
	case 'j':
		options->lengthMode = 5;
		c                   = fmt[++offset];
		break;
	case 'z':
		options->lengthMode = 6;
		c                   = fmt[++offset];
		break;
	case 't':
		options->lengthMode = 7;
		c                   = fmt[++offset];
		break;
	case 'L':
		options->lengthMode = 8;
		c                   = fmt[++offset];
		break;
	}

	switch (c)
	{
	case '%':
	case 'b':
	case 'c':
	case 's':
	case 'd':
	case 'i':
	case 'o':
	case 'x':
	case 'X':
	case 'u':
	case 'f':
	case 'F':
	case 'e':
	case 'E':
	case 'a':
	case 'A':
	case 'g':
	case 'G':
	case 'n':
	case 'p':
		options->formatChar = c;
		++offset;
		break;
	}

	return offset;
}

static inline bool WritePadding(FILE* restrict stream, char padChar, size_t count, size_t* len)
{
	if (!stream)
	{
		*len += count;
		return true;
	}

	for (size_t i = 0; i < count; ++i)
	{
		if (!stream->WriteChar(stream, padChar))
		{
			*len += i;
			return false;
		}
	}
	*len += count;
	return true;
}

static inline bool LeftSpacePad(FILE* restrict stream, struct FormatOptions* options, size_t fieldLen, size_t* len)
{
	if (options->leftJustified || options->fieldWidth <= fieldLen)
		return true;
	return WritePadding(stream, ' ', options->fieldWidth - fieldLen, len);
}

static inline bool RightSpacePad(FILE* restrict stream, struct FormatOptions* options, size_t fieldLen, size_t* len)
{
	if (!options->leftJustified || options->fieldWidth <= fieldLen)
		return true;
	return WritePadding(stream, ' ', options->fieldWidth - fieldLen, len);
}

size_t vfscanf(FILE* restrict stream, const char* restrict fmt, va_list vlist)
{
	size_t len = 0;
	return len;
}

size_t vfprintf(FILE* restrict stream, const char* restrict fmt, va_list vlist)
{
	size_t len = 0;

	size_t fmtOffset = 0;
	while (1)
	{
		size_t fmtBegin = StrFindFirst(fmt, '%', fmtOffset);
		if (stream)
		{
			size_t written = stream->WriteChars(stream, fmt + fmtOffset, fmtBegin - fmtOffset);
			len           += written;
			if (written != fmtBegin - fmtOffset)
				break;
		}
		else
		{
			len += fmtBegin - fmtOffset;
		}
		if (fmt[fmtBegin] == '\0')
			break;

		struct FormatOptions options;
		fmtOffset = ParseFormatOptions(fmt, fmtBegin + 1, &options, (va_list*) &vlist);
		switch (options.formatChar)
		{
		case '%':
			if (!LeftSpacePad(stream, &options, 1, &len))
				goto BREAKOUT;
			if (stream && !stream->WriteChar(stream, '%'))
				goto BREAKOUT;
			++len;
			if (!RightSpacePad(stream, &options, 1, &len))
				goto BREAKOUT;
			break;
		case 'b':
		{
			int b = -1;
			switch (options.lengthMode)
			{
			case 0:
			case 1:
			case 2: b = va_arg(vlist, int); break;
			}
			b = !!b;
			if (!LeftSpacePad(stream, &options, b ? 4 : 5, &len))
				goto BREAKOUT;
			if (!stream->WriteChars(stream, b ? "true" : "false", b ? 4 : 5))
				goto BREAKOUT;
			len += b ? 4 : 5;
			if (!RightSpacePad(stream, &options, b ? 4 : 5, &len))
				goto BREAKOUT;
			break;
		}
		case 'c':
		{
			int c = -1;
			switch (options.lengthMode)
			{
			case 0:
			case 1:
			case 2: c = va_arg(vlist, int); break;
			}
			bool valid = c >= 0 && c <= 255;
			if (!LeftSpacePad(stream, &options, valid, &len))
				goto BREAKOUT;
			if (valid && stream && !stream->WriteChar(stream, (char) c))
				goto BREAKOUT;
			len += valid;
			if (!RightSpacePad(stream, &options, valid, &len))
				goto BREAKOUT;
			break;
		}
		case 's':
		{
			switch (options.lengthMode)
			{
			case 0:
			case 1:
			case 2:
			{
				const char* str     = va_arg(vlist, const char*);
				size_t      toWrite = strlen(str);
				if (options.precision >= 0)
					toWrite = toWrite < options.precision ? toWrite : options.precision;
				if (!LeftSpacePad(stream, &options, toWrite, &len))
					goto BREAKOUT;
				if (stream)
				{
					size_t written = stream->WriteChars(stream, str, toWrite);
					len           += written;
					if (written != toWrite)
						goto BREAKOUT;
				}
				else
				{
					len += toWrite;
				}
				if (!RightSpacePad(stream, &options, toWrite, &len))
					goto BREAKOUT;
				break;
			}
			}
			break;
		}
		case 'd':
		case 'i':
		{
			intmax_t v = 0;
			switch (options.lengthMode)
			{
			case 0:
			case 1:
			case 2: v = (intmax_t) va_arg(vlist, int); break;
			case 3: v = (intmax_t) va_arg(vlist, long); break;
			case 4: v = (intmax_t) va_arg(vlist, long long); break;
			case 5: v = (intmax_t) va_arg(vlist, intmax_t); break;
			case 6: v = (intmax_t) va_arg(vlist, ssize_t); break;
			case 7: v = (intmax_t) va_arg(vlist, ptrdiff_t); break;
			}
			if (options.precision == 0 && v == 0)
				break;

			char   buf[21];
			bool   isNegative = v < 0;
			size_t start      = 20;
			buf[20]           = '\0';
			if (isNegative)
				v = -v;
			while (v)
			{
				intmax_t rem = v % 10;
				v           /= 10;
				buf[--start] = '0' + rem;
			}

			char signChar = '\0';

			size_t fieldLen     = 20 - start;
			size_t zeroPadCount = 0;
			if (options.precision >= 0)
			{
				if (fieldLen < options.precision)
				{
					zeroPadCount = options.precision - fieldLen;
					fieldLen     = options.precision;
				}
			}
			else if (options.zeroPad &&
					 !options.leftJustified &&
					 fieldLen < options.fieldWidth)
			{
				zeroPadCount = options.fieldWidth - fieldLen;
				fieldLen     = options.fieldWidth;
			}
			else if (fieldLen == 0)
			{
				zeroPadCount = 1;
				fieldLen     = 1;
			}

			switch (options.signMode)
			{
			case 0:
				if (isNegative)
				{
					signChar = '-';
					if (zeroPadCount > 0)
						--zeroPadCount;
					else
						++fieldLen;
				}
				break;
			case 1:
				signChar = isNegative ? '-' : '+';
				if (zeroPadCount > 0)
					--zeroPadCount;
				else
					++fieldLen;
				break;
			case 2:
				signChar = isNegative ? '-' : ' ';
				if (zeroPadCount > 0)
					--zeroPadCount;
				else
					++fieldLen;
				break;
			}

			if (!LeftSpacePad(stream, &options, fieldLen, &len))
				goto BREAKOUT;
			if (signChar != '\0')
			{
				if (stream && !stream->WriteChar(stream, signChar))
					goto BREAKOUT;
				++len;
			}
			if (!WritePadding(stream, '0', zeroPadCount, &len))
				goto BREAKOUT;
			if (stream)
			{
				size_t written = stream->WriteChars(stream, buf + start, 20 - start);
				len           += written;
				if (written != 20 - start)
					goto BREAKOUT;
			}
			if (!RightSpacePad(stream, &options, fieldLen, &len))
				goto BREAKOUT;
			break;
		}
		case 'u':
		{
			uintmax_t v = 0;
			switch (options.lengthMode)
			{
			case 0:
			case 1:
			case 2: v = (uintmax_t) va_arg(vlist, unsigned int); break;
			case 3: v = (uintmax_t) va_arg(vlist, unsigned long); break;
			case 4: v = (uintmax_t) va_arg(vlist, unsigned long long); break;
			case 5: v = (uintmax_t) va_arg(vlist, uintmax_t); break;
			case 6: v = (uintmax_t) va_arg(vlist, size_t); break;
			case 7: v = (uintmax_t) va_arg(vlist, uptrdiff_t); break;
			}
			if (options.precision == 0 && v == 0)
				break;

			char   buf[21];
			size_t start = 20;
			buf[20]      = '\0';
			while (v)
			{
				uintmax_t rem = v % 10;
				v            /= 10;
				buf[--start]  = '0' + rem;
			}

			size_t fieldLen     = 20 - start;
			size_t zeroPadCount = 0;
			if (options.precision >= 0)
			{
				if (fieldLen < options.precision)
				{
					zeroPadCount = options.precision - fieldLen;
					fieldLen     = options.precision;
				}
			}
			else if (options.zeroPad &&
					 !options.leftJustified &&
					 fieldLen < options.fieldWidth)
			{
				zeroPadCount = options.fieldWidth - fieldLen;
				fieldLen     = options.fieldWidth;
			}
			else if (fieldLen == 0)
			{
				zeroPadCount = 1;
				fieldLen     = 1;
			}

			if (!LeftSpacePad(stream, &options, fieldLen, &len))
				goto BREAKOUT;
			if (!WritePadding(stream, '0', zeroPadCount, &len))
				goto BREAKOUT;
			if (stream)
			{
				size_t written = stream->WriteChars(stream, buf + start, 20 - start);
				len           += written;
				if (written != 20 - start)
					goto BREAKOUT;
			}
			if (!RightSpacePad(stream, &options, fieldLen, &len))
				goto BREAKOUT;
			break;
		}
		case 'o':
		{
			uintmax_t v = 0;
			switch (options.lengthMode)
			{
			case 0:
			case 1:
			case 2: v = (uintmax_t) va_arg(vlist, unsigned int); break;
			case 3: v = (uintmax_t) va_arg(vlist, unsigned long); break;
			case 4: v = (uintmax_t) va_arg(vlist, unsigned long long); break;
			case 5: v = (uintmax_t) va_arg(vlist, uintmax_t); break;
			case 6: v = (uintmax_t) va_arg(vlist, size_t); break;
			case 7: v = (uintmax_t) va_arg(vlist, uptrdiff_t); break;
			}
			if (options.precision == 0 && v == 0)
				break;

			char   buf[23];
			size_t start = 22;
			buf[22]      = '\0';
			while (v)
			{
				uintmax_t rem = v % 8;
				v            /= 8;
				buf[--start]  = '0' + rem;
			}

			size_t fieldLen     = 22 - start;
			size_t zeroPadCount = 0;
			if (options.precision >= 0)
			{
				if (fieldLen < options.precision)
				{
					zeroPadCount = options.precision - fieldLen;
					fieldLen     = options.precision;
				}
			}
			else if (options.zeroPad &&
					 !options.leftJustified &&
					 fieldLen < options.fieldWidth)
			{
				zeroPadCount = options.fieldWidth - fieldLen;
				fieldLen     = options.fieldWidth;
			}
			else if (fieldLen == 0)
			{
				zeroPadCount = 1;
				fieldLen     = 1;
			}

			if (options.altMode && zeroPadCount == 0)
			{
				++zeroPadCount;
				++fieldLen;
			}

			if (!LeftSpacePad(stream, &options, fieldLen, &len))
				goto BREAKOUT;
			if (!WritePadding(stream, '0', zeroPadCount, &len))
				goto BREAKOUT;
			if (stream)
			{
				size_t written = stream->WriteChars(stream, buf + start, 22 - start);
				len           += written;
				if (written != 22 - start)
					goto BREAKOUT;
			}
			if (!RightSpacePad(stream, &options, fieldLen, &len))
				goto BREAKOUT;
			break;
		}
		case 'x':
		case 'X':
		{
			bool      uppercase = options.formatChar == 'X';
			uintmax_t v         = 0;
			switch (options.lengthMode)
			{
			case 0:
			case 1:
			case 2: v = (uintmax_t) va_arg(vlist, unsigned int); break;
			case 3: v = (uintmax_t) va_arg(vlist, unsigned long); break;
			case 4: v = (uintmax_t) va_arg(vlist, unsigned long long); break;
			case 5: v = (uintmax_t) va_arg(vlist, uintmax_t); break;
			case 6: v = (uintmax_t) va_arg(vlist, size_t); break;
			case 7: v = (uintmax_t) va_arg(vlist, uptrdiff_t); break;
			}
			if (options.precision == 0 && v == 0)
				break;

			char   buf[17];
			size_t start = 16;
			buf[16]      = '\0';
			while (v)
			{
				uintmax_t rem = v & 0xF;
				v           >>= 4;
				if (rem < 10)
					buf[--start] = '0' + rem;
				else
					buf[--start] = (uppercase ? 'A' : 'a') + (rem - 10);
			}

			size_t fieldLen     = 16 - start;
			size_t zeroPadCount = 0;
			if (options.precision >= 0)
			{
				if (fieldLen < options.precision)
				{
					zeroPadCount = options.precision - fieldLen;
					fieldLen     = options.precision;
				}
			}
			else if (options.zeroPad &&
					 !options.leftJustified &&
					 fieldLen < options.fieldWidth)
			{
				zeroPadCount = options.fieldWidth - fieldLen;
				fieldLen     = options.fieldWidth;
			}
			else if (fieldLen == 0)
			{
				zeroPadCount = 1;
				fieldLen     = 1;
			}

			if (options.altMode)
			{
				switch (zeroPadCount)
				{
				case 0:
					fieldLen += 2;
					break;
				case 1:
					--zeroPadCount;
					++fieldLen;
					break;
				default:
					zeroPadCount -= 2;
					break;
				}
			}

			if (!LeftSpacePad(stream, &options, fieldLen, &len))
				goto BREAKOUT;
			if (options.altMode)
			{
				if (stream)
				{
					if (!stream->WriteChar(stream, '0'))
						goto BREAKOUT;
					++len;
					if (!stream->WriteChar(stream, uppercase ? 'X' : 'x'))
						goto BREAKOUT;
					++len;
				}
				else
				{
					len += 2;
				}
			}
			if (!WritePadding(stream, '0', zeroPadCount, &len))
				goto BREAKOUT;
			if (stream)
			{
				size_t written = stream->WriteChars(stream, buf + start, 16 - start);
				len           += written;
				if (written != 16 - start)
					goto BREAKOUT;
			}
			if (!RightSpacePad(stream, &options, fieldLen, &len))
				goto BREAKOUT;
			break;
		}
		case 'f':
		case 'F':
		case 'e':
		case 'E':
		case 'a':
		case 'A':
		case 'g':
		case 'G':
		{
			double v = 0;
			switch (options.lengthMode)
			{
			case 0:
			case 1:
			case 2:
			case 3: v = va_arg(vlist, double); break;
			case 4:
			case 5:
			case 6:
			case 7: break;
			case 8: v = va_arg(vlist, double); break;
			}
			break;
		}
		case 'n':
			switch (options.lengthMode)
			{
			case 0:
			{
				signed char* pn = va_arg(vlist, signed char*);
				if (pn)
					*pn = (signed char) len;
				break;
			}
			case 1:
			{
				short* pn = va_arg(vlist, short*);
				if (pn)
					*pn = (short) len;
				break;
			}
			case 2:
			{
				int* pn = va_arg(vlist, int*);
				if (pn)
					*pn = (int) len;
				break;
			}
			case 3:
			{
				long* pn = va_arg(vlist, long*);
				if (pn)
					*pn = (long) len;
				break;
			}
			case 4:
			{
				long long* pn = va_arg(vlist, long long*);
				if (pn)
					*pn = (long long) len;
				break;
			}
			case 5:
			{
				intmax_t* pn = va_arg(vlist, intmax_t*);
				if (pn)
					*pn = (intmax_t) len;
				break;
			}
			case 6:
			{
				ssize_t* pn = va_arg(vlist, ssize_t*);
				if (pn)
					*pn = (ssize_t) len;
				break;
			}
			case 7:
			{
				ptrdiff_t* pn = va_arg(vlist, ptrdiff_t*);
				if (pn)
					*pn = (ptrdiff_t) len;
				break;
			}
			}
			break;
		case 'p':
		{
			options.precision = 16;
			uintmax_t v       = 0;
			switch (options.lengthMode)
			{
			case 0:
			case 1:
			case 2: v = (uintmax_t) va_arg(vlist, void*); break;
			}

			char   buf[17];
			size_t start = 16;
			buf[16]      = '\0';
			while (v)
			{
				uintmax_t rem = v & 0xF;
				v           >>= 4;
				if (rem < 10)
					buf[--start] = '0' + rem;
				else
					buf[--start] = 'A' + (rem - 10);
			}

			size_t fieldLen     = 16 - start;
			size_t zeroPadCount = 0;
			if (options.precision >= 0)
			{
				if (fieldLen < options.precision)
				{
					zeroPadCount = options.precision - fieldLen;
					fieldLen     = options.precision;
				}
			}
			else if (options.zeroPad &&
					 !options.leftJustified &&
					 fieldLen < options.fieldWidth)
			{
				zeroPadCount = options.fieldWidth - fieldLen;
				fieldLen     = options.fieldWidth;
			}
			else if (fieldLen == 0)
			{
				zeroPadCount = 1;
				fieldLen     = 1;
			}

			if (options.altMode)
			{
				switch (zeroPadCount)
				{
				case 0:
					fieldLen += 2;
					break;
				case 1:
					--zeroPadCount;
					++fieldLen;
					break;
				default:
					zeroPadCount -= 2;
					break;
				}
			}

			if (!LeftSpacePad(stream, &options, fieldLen, &len))
				goto BREAKOUT;
			if (options.altMode)
			{
				if (stream)
				{
					if (!stream->WriteChar(stream, '0'))
						goto BREAKOUT;
					++len;
					if (!stream->WriteChar(stream, 'x'))
						goto BREAKOUT;
					++len;
				}
				else
				{
					len += 2;
				}
			}
			if (!WritePadding(stream, '0', zeroPadCount, &len))
				goto BREAKOUT;
			if (stream)
			{
				size_t written = stream->WriteChars(stream, buf + start, 16 - start);
				len           += written;
				if (written != 16 - start)
					goto BREAKOUT;
			}
			if (!RightSpacePad(stream, &options, fieldLen, &len))
				goto BREAKOUT;
			break;
		}
		}

		continue;
	BREAKOUT:
		break;
	}

	return len;
}