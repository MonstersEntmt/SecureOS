#include "stdio.h"
#include "string.h"

static const char s_PadBuffer[]     = "                                                                                                                                ";
static const char s_ZeroPadBuffer[] = "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";
static const char s_HexLowercase[]  = "0123456789abcdef";
static const char s_HexUppercase[]  = "0123456789ABCDEF";

FILE* __stdin  = nullptr;
FILE* __stdout = nullptr;
FILE* __stderr = nullptr;

FILE* __FILE_REDIRECT_STDIN(FILE* restrict stream)
{
	FILE* original = __stdin;
	__stdin        = stream;
	return original;
}

FILE* __FILE_REDIRECT_STDOUT(FILE* restrict stream)
{
	FILE* original = __stdout;
	__stdout       = stream;
	return original;
}

FILE* __FILE_REDIRECT_STDERR(FILE* restrict stream)
{
	FILE* original = __stderr;
	__stderr       = stream;
	return original;
}

static bool __FILE_BUFFER_WRITE_CHAR(struct __FILE* restrict stream, char c)
{
	if (stream->Position >= stream->Userdatas[0])
	{
		stream->State |= __FILE_STATE_EOF;
		return false;
	}

	((char*) stream->Userdatas[1])[stream->Position] = c;
	if (++stream->Position >= stream->Userdatas[0])
		stream->State |= __FILE_STATE_EOF;
	return true;
}

static int __FILE_BUFFER_READ_CHAR(struct __FILE* restrict stream)
{
	if (stream->Position >= stream->Userdatas[0])
	{
		stream->State |= __FILE_STATE_EOF;
		return EOF;
	}

	int ch = ((char*) stream->Userdatas[1])[stream->Position];
	if (++stream->Position >= stream->Userdatas[0])
		stream->State |= __FILE_STATE_EOF;
	return ch;
}

static ssize_t __FILE_BUFFER_WRITE_CHARS(struct __FILE* restrict stream, const char* restrict str, size_t count)
{
	if (stream->Position >= stream->Userdatas[0])
	{
		stream->State |= __FILE_STATE_EOF;
		return EOF;
	}

	ssize_t toCopy = count < stream->Userdatas[0] - stream->Position ? count : stream->Userdatas[0] - stream->Position;
	memcpy((void*) stream->Userdatas[1], str, toCopy);
	if (stream->Position += toCopy >= stream->Userdatas[0])
		stream->State |= __FILE_STATE_EOF;
	return toCopy;
}

static ssize_t __FILE_BUFFER_READ_CHARS(struct __FILE* restrict stream, char* restrict str, size_t count)
{
	if (stream->Position >= stream->Userdatas[0])
	{
		stream->State |= __FILE_STATE_EOF;
		return EOF;
	}

	ssize_t toCopy = count < stream->Userdatas[0] - stream->Position ? count : stream->Userdatas[0] - stream->Position;
	memcpy(str, (const void*) stream->Userdatas[1], toCopy);
	if (stream->Position += toCopy >= stream->Userdatas[0])
		stream->State |= __FILE_STATE_EOF;
	return toCopy;
}

static bool __FILE_BUFFER_SEEK(struct __FILE* restrict stream, ssize_t offset, int origin)
{
	switch (origin)
	{
	case SEEK_SET:
		if (offset < 0)
			return false;
		if (offset >= stream->Userdatas[0])
		{
			stream->Position = stream->Userdatas[0];
			stream->State   |= __FILE_STATE_EOF;
			return false;
		}
		stream->Position = (size_t) offset;
		stream->State   &= ~__FILE_STATE_EOF;
		return true;
	case SEEK_CUR:
		if (offset < 0)
		{
			if (-offset > stream->Position)
			{
				stream->Position = 0;
				stream->State   &= ~__FILE_STATE_EOF;
				return false;
			}
			stream->Position += offset;
			stream->State    &= ~__FILE_STATE_EOF;
			return true;
		}
		if (stream->Position + offset >= stream->Userdatas[0])
		{
			stream->Position = stream->Userdatas[0];
			stream->State   |= __FILE_STATE_EOF;
			return false;
		}
		stream->Position += offset;
		stream->State    &= ~__FILE_STATE_EOF;
		return true;
	case SEEK_END:
		if (offset < 0)
			return false;
		if (offset >= stream->Userdatas[0])
		{
			stream->Position = 0;
			stream->State   &= ~__FILE_STATE_EOF;
			return false;
		}
		stream->Position = stream->Userdatas[0] - (size_t) offset;
		if (stream->Position >= stream->Userdatas[0])
			stream->State |= __FILE_STATE_EOF;
		else
			stream->State &= ~__FILE_STATE_EOF;
		return true;
	default:
		return false;
	}
}

void __FILE_READ_BUFFER(FILE* restrict stream, const void* buf, size_t size)
{
	if (!stream)
		return;
	memset(stream, 0, sizeof(FILE));
	stream->ReadChar     = &__FILE_BUFFER_READ_CHAR;
	stream->ReadChars    = &__FILE_BUFFER_READ_CHARS;
	stream->Seek         = &__FILE_BUFFER_SEEK;
	stream->Userdatas[0] = size;
	stream->Userdatas[1] = (uint64_t) buf;
}

void __FILE_WRITE_BUFFER(FILE* restrict stream, void* buf, size_t size)
{
	if (!stream)
		return;
	memset(stream, 0, sizeof(FILE));
	stream->WriteChar    = &__FILE_BUFFER_WRITE_CHAR;
	stream->WriteChars   = &__FILE_BUFFER_WRITE_CHARS;
	stream->Seek         = &__FILE_BUFFER_SEEK;
	stream->Userdatas[0] = size;
	stream->Userdatas[1] = (uint64_t) buf;
}

void __FILE_READ_WRITE_BUFFER(FILE* restrict stream, void* buf, size_t size)
{
	if (!stream)
		return;
	memset(stream, 0, sizeof(FILE));
	stream->WriteChar    = &__FILE_BUFFER_WRITE_CHAR;
	stream->ReadChar     = &__FILE_BUFFER_READ_CHAR;
	stream->WriteChars   = &__FILE_BUFFER_WRITE_CHARS;
	stream->ReadChars    = &__FILE_BUFFER_READ_CHARS;
	stream->Seek         = &__FILE_BUFFER_SEEK;
	stream->Userdatas[0] = size;
	stream->Userdatas[1] = (uint64_t) buf;
}

ssize_t ftell(FILE* restrict stream)
{
	if (!stream)
		return -1;
	return stream->Position;
}

int fseek(FILE* restrict stream, ssize_t offset, int origin)
{
	if (!stream || !stream->Seek)
		return 1;
	return stream->Seek(stream, offset, origin) ? 0 : 1;
}

void frewind(FILE* restrict stream)
{
	fseek(stream, 0, SEEK_SET);
}

int fgetc(FILE* restrict stream)
{
	if (!stream)
		return EOF;
	if (!stream->ReadChar)
	{
		stream->State |= __FILE_STATE_EOF;
		return EOF;
	}
	return stream->ReadChar(stream);
}

char* fgets(char* restrict str, size_t count, FILE* restrict stream)
{
	if (!str || !stream)
		return nullptr;
	if (!stream->ReadChar)
		return nullptr;
	if (!count)
		return str;
	size_t i = 0;
	for (; i < count - 1; ++i)
	{
		int ch = stream->ReadChar(stream);
		if (ch < 0)
			break;
		str[i] = (char) ch;
		if (ch == '\n')
			break;
	}
	if (i > 0)
		str[i] = '\0';
	return i > 0 ? str : nullptr;
}

ssize_t fread(void* restrict buf, size_t size, size_t count, FILE* restrict stream)
{
	if (!buf || !stream)
		return -1;
	if (!stream->ReadChars)
		return -1;
	ssize_t res = stream->ReadChars(stream, buf, size * count);
	if (res < 0)
		return -1;
	ssize_t readCount = res / size;
	ssize_t remainder = res % size;
	if (remainder > 0 && stream->Seek)
		stream->Seek(stream, -remainder, SEEK_CUR);
	return readCount;
}

bool fputc(int ch, FILE* restrict stream)
{
	if (!stream || ch < 0 || ch > 255)
		return false;
	if (!stream->WriteChar)
		return false;
	return stream->WriteChar(stream, (char) ch);
}

bool fputs(const char* restrict str, FILE* restrict stream)
{
	size_t  len = strlen(str);
	ssize_t res = fwrite(str, 1, len, stream);
	return len == res;
}

ssize_t fwrite(const void* restrict buf, size_t size, size_t count, FILE* restrict stream)
{
	if (!buf || !stream)
		return -1;
	if (!stream->WriteChars)
		return -1;
	ssize_t res = stream->WriteChars(stream, buf, size * count);
	if (res < 0)
		return -1;
	ssize_t writeCount = res / size;
	ssize_t remainder  = res % size;
	if (remainder > 0 && stream->Seek)
		stream->Seek(stream, -remainder, SEEK_CUR);
	return writeCount;
}

int fflush(FILE* restrict stream)
{
	if (!stream)
		return 1;
	if (!stream->Flush)
		return 0;
	return stream->Flush(stream) ? 0 : 1;
}

bool feof(FILE* restrict stream)
{
	return stream ? (stream->State & __FILE_STATE_EOF) != 0 : true;
}

bool ferror(FILE* restrict stream)
{
	return stream ? (stream->State & __FILE_STATE_ERROR) != 0 : true;
}

int getc(void)
{
	return fgetc(stdin);
}

char* gets(char* restrict str, size_t count)
{
	FILE* stream = stdin;
	if (!str || !stream)
		return nullptr;
	if (!stream->ReadChar)
		return nullptr;
	if (!count)
		return str;
	size_t i = 0;
	while (true)
	{
		int ch = stream->ReadChar(stream);
		if (ch < 0 || ch == '\n')
			break;
		if (i >= count - 1)
			break;
		str[i] = (char) ch;
		++i;
	}
	if (i > 0)
		str[i] = '\0';
	return i > 0 ? str : nullptr;
}

bool putc(int ch)
{
	return fputc(ch, stdout);
}

bool puts(const char* restrict str)
{
	FILE* stream = stdout;
	if (!fputs(str, stream) ||
		!fputc('\n', stream))
		return false;
	return true;
}

ssize_t scanf(const char* restrict fmt, ...)
{
	if (!stdin)
		return 0;
	va_list vlist;
	va_start(vlist, fmt);
	ssize_t res = vfscanf(stdin, fmt, vlist);
	va_end(vlist);
	return res;
}

ssize_t vscanf(const char* restrict fmt, va_list vlist)
{
	if (!stdin)
		return 0;
	return vfscanf(stdin, fmt, vlist);
}

ssize_t sscanf(const char* restrict buf, const char* restrict fmt, ...)
{
	FILE file;
	if (buf)
		__FILE_READ_BUFFER(&file, buf, strlen(buf));

	va_list vlist;
	va_start(vlist, fmt);
	ssize_t res = vfscanf(buf ? &file : nullptr, fmt, vlist);
	va_end(vlist);
	return res;
}

ssize_t vsscanf(const char* restrict buf, const char* restrict fmt, va_list vlist)
{
	FILE file;
	if (buf)
		__FILE_READ_BUFFER(&file, buf, strlen(buf));

	return vfscanf(buf ? &file : nullptr, fmt, vlist);
}

ssize_t snscanf(const char* restrict buf, size_t bufSize, const char* restrict fmt, ...)
{
	FILE file;
	if (buf)
		__FILE_READ_BUFFER(&file, buf, bufSize);

	va_list vlist;
	va_start(vlist, fmt);
	ssize_t res = vfscanf(buf ? &file : nullptr, fmt, vlist);
	va_end(vlist);
	return res;
}

ssize_t vsnscanf(const char* restrict buf, size_t bufSize, const char* restrict fmt, va_list vlist)
{
	FILE file;
	if (buf)
		__FILE_READ_BUFFER(&file, buf, bufSize);

	return vfscanf(buf ? &file : nullptr, fmt, vlist);
}

ssize_t fscanf(FILE* restrict stream, const char* restrict fmt, ...)
{
	va_list vlist;
	va_start(vlist, fmt);
	ssize_t res = vfscanf(stream, fmt, vlist);
	va_end(vlist);
	return res;
}

ssize_t printf(const char* restrict fmt, ...)
{
	if (!stdout)
		return 0;
	va_list vlist;
	va_start(vlist, fmt);
	ssize_t res = vfprintf(stdout, fmt, vlist);
	va_end(vlist);
	return res;
}

ssize_t vprintf(const char* restrict fmt, va_list vlist)
{
	if (!stdout)
		return 0;
	return vfprintf(stdout, fmt, vlist);
}

ssize_t sprintf(char* restrict buf, const char* restrict fmt, ...)
{
	FILE file;
	if (buf)
		__FILE_WRITE_BUFFER(&file, buf, ~0ULL); // INFO(MarcasRealAccount): As we can't assume buf is filled with non null terminating characters we can't figure out the true size of the buffer.

	va_list vlist;
	va_start(vlist, fmt);
	ssize_t res = vfprintf(buf ? &file : nullptr, fmt, vlist);
	va_end(vlist);
	return res;
}

ssize_t vsprintf(char* restrict buf, const char* restrict fmt, va_list vlist)
{
	FILE file;
	if (buf)
		__FILE_WRITE_BUFFER(&file, buf, ~0ULL);

	return vfprintf(buf ? &file : nullptr, fmt, vlist);
}

ssize_t snprintf(char* restrict buf, size_t bufSize, const char* restrict fmt, ...)
{
	FILE file;
	if (buf)
		__FILE_WRITE_BUFFER(&file, buf, bufSize);

	va_list vlist;
	va_start(vlist, fmt);
	ssize_t res = vfprintf(buf ? &file : nullptr, fmt, vlist);
	va_end(vlist);
	return res;
}

ssize_t vsnprintf(char* restrict buf, size_t bufSize, const char* restrict fmt, va_list vlist)
{
	FILE file;
	if (buf)
		__FILE_WRITE_BUFFER(&file, buf, bufSize);

	return vfprintf(buf ? &file : nullptr, fmt, vlist);
}

ssize_t fprintf(FILE* restrict stream, const char* restrict fmt, ...)
{
	va_list vlist;
	va_start(vlist, fmt);
	ssize_t res = vfprintf(stream, fmt, vlist);
	va_end(vlist);
	return res;
}

struct FormatOptions
{
	bool     LeftJustified;
	uint8_t  SignMode;
	bool     AltMode;
	bool     ZeroPad;
	uint32_t FieldWidth;
	int32_t  Precision;
	uint8_t  LengthMode;
	char     FormatChar;
};

static inline size_t ParseFormatOptions(const char* restrict fmt, size_t offset, struct FormatOptions* options, va_list* vlist)
{
	*options = (struct FormatOptions) {
		.LeftJustified = false,
		.SignMode      = 0,
		.AltMode       = false,
		.ZeroPad       = false,
		.FieldWidth    = 0,
		.Precision     = -1,
		.LengthMode    = 2,
		.FormatChar    = '\0'
	};

	// Parse primary options
	char c = fmt[offset];
	while (1)
	{
		switch (c)
		{
		case '-':
			options->LeftJustified = true;
			c                      = fmt[++offset];
			break;
		case '+':
			options->SignMode = 1;
			c                 = fmt[++offset];
			break;
		case ' ':
			options->SignMode = 2;
			c                 = fmt[++offset];
			break;
		case '#':
			options->AltMode = true;
			c                = fmt[++offset];
			break;
		case '0':
			options->ZeroPad = true;
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
			options->FieldWidth *= 10;
			options->FieldWidth += c - '0';
			c                    = fmt[++offset];
			break;
		case '*':
			options->FieldWidth = (uint32_t) va_arg(*vlist, int);
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
		options->Precision = 0;
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
				options->Precision *= 10;
				options->Precision += c - '0';
				c                   = fmt[++offset];
				break;
			case '*':
				options->Precision = (uint32_t) va_arg(*vlist, int);
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
			options->LengthMode = 0;
			c                   = fmt[++offset];
			break;
		default:
			options->LengthMode = 1;
			break;
		}
		break;
	case 'l':
		c = fmt[++offset];
		switch (c)
		{
		case 'l':
			options->LengthMode = 4;
			c                   = fmt[++offset];
			break;
		default:
			options->LengthMode = 3;
			break;
		}
		break;
	case 'j':
		options->LengthMode = 5;
		c                   = fmt[++offset];
		break;
	case 'z':
		options->LengthMode = 6;
		c                   = fmt[++offset];
		break;
	case 't':
		options->LengthMode = 7;
		c                   = fmt[++offset];
		break;
	case 'L':
		options->LengthMode = 8;
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
		options->FormatChar = c;
		++offset;
		break;
	}

	return offset;
}

static inline bool WriteZeroPadding(FILE* restrict stream, size_t toPad, size_t* len)
{
	if (!stream)
	{
		*len += toPad;
		return true;
	}
	size_t count  = toPad / (sizeof(s_ZeroPadBuffer) - 1);
	size_t remain = toPad % (sizeof(s_ZeroPadBuffer) - 1);
	for (size_t i = 0; i < count; ++i)
	{
		ssize_t res = stream->WriteChars(stream, s_ZeroPadBuffer, sizeof(s_ZeroPadBuffer) - 1);
		if (res < 0)
			return false;
		*len += (size_t) res;
	}
	if (remain > 0)
	{
		ssize_t res = stream->WriteChars(stream, s_ZeroPadBuffer, remain);
		if (res < 0)
			return false;
		*len += (size_t) res;
	}
	return true;
}

static inline bool WritePadding(FILE* restrict stream, size_t toPad, size_t* len)
{
	if (!stream)
	{
		*len += toPad;
		return true;
	}
	size_t count  = toPad / (sizeof(s_PadBuffer) - 1);
	size_t remain = toPad % (sizeof(s_PadBuffer) - 1);
	for (size_t i = 0; i < count; ++i)
	{
		ssize_t res = stream->WriteChars(stream, s_PadBuffer, sizeof(s_PadBuffer) - 1);
		if (res < 0)
			return false;
		*len += (size_t) res;
	}
	if (remain > 0)
	{
		ssize_t res = stream->WriteChars(stream, s_PadBuffer, remain);
		if (res < 0)
			return false;
		*len += (size_t) res;
	}
	return true;
}

static inline bool LeftSpacePad(FILE* restrict stream, struct FormatOptions* options, size_t fieldLen, size_t* len)
{
	if (options->LeftJustified || options->FieldWidth <= fieldLen)
		return true;
	return WritePadding(stream, options->FieldWidth - fieldLen, len);
}

static inline bool RightSpacePad(FILE* restrict stream, struct FormatOptions* options, size_t fieldLen, size_t* len)
{
	if (!options->LeftJustified || options->FieldWidth <= fieldLen)
		return true;
	return WritePadding(stream, options->FieldWidth - fieldLen, len);
}

ssize_t vfscanf(FILE* restrict stream, const char* restrict fmt, va_list vlist)
{
	return 0;
}

ssize_t vfprintf(FILE* restrict stream, const char* restrict fmt, va_list vlist)
{
	if (stream && (!stream->WriteChar || !stream->WriteChars))
		return -1;

	size_t len       = 0;
	size_t fmtOffset = 0;
	size_t fmtLen    = strlen(fmt);
	while (1)
	{
		const char* fmtBegin = (const char*) memchr(fmt + fmtOffset, '%', fmtLen - fmtOffset);
		if (stream)
		{
			size_t  toWrite = fmtBegin - (fmt + fmtOffset);
			ssize_t res     = stream->WriteChars(stream, fmt + fmtOffset, toWrite);
			if (res < 0)
				break;
			len += res;
			if (res < toWrite)
				break;
		}
		else
		{
			len += fmtBegin - (fmt + fmtOffset);
		}
		if (*fmtBegin == '\0')
			break;

		struct FormatOptions options;
		fmtOffset = ParseFormatOptions(fmt, fmtBegin - fmt + 1, &options, (va_list*) &vlist);
		switch (options.FormatChar)
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
			switch (options.LengthMode)
			{
			case 0:
			case 1:
			case 2: b = va_arg(vlist, int); break;
			}
			b = !!b;
			if (!LeftSpacePad(stream, &options, b ? 4 : 5, &len))
				goto BREAKOUT;
			if (stream)
			{
				ssize_t res = stream->WriteChars(stream, b ? "true" : "false", b ? 4 : 5);
				if (res < 0)
					goto BREAKOUT;
				len += res;
				if (res < (b ? 4 : 5))
					goto BREAKOUT;
			}
			else
			{
				len += b ? 4 : 5;
			}
			if (!RightSpacePad(stream, &options, b ? 4 : 5, &len))
				goto BREAKOUT;
			break;
		}
		case 'c':
		{
			int c = -1;
			switch (options.LengthMode)
			{
			case 0:
			case 1:
			case 2: va_arg(vlist, int); break;
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
			switch (options.LengthMode)
			{
			case 0:
			case 1:
			case 2:
			{
				const char* str     = va_arg(vlist, const char*);
				size_t      toWrite = strlen(str);
				if (options.Precision >= 0)
					toWrite = toWrite < options.Precision ? toWrite : options.Precision;
				if (!LeftSpacePad(stream, &options, toWrite, &len))
					goto BREAKOUT;
				if (stream)
				{
					ssize_t res = stream->WriteChars(stream, str, toWrite);
					if (res < 0)
						goto BREAKOUT;
					len += res;
					if (res < toWrite)
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
			case 8:
			{
				const char* subFmt   = va_arg(vlist, const char*);
				va_list*    subVlist = va_arg(vlist, va_list*);
				len                 += vfprintf(stream, subFmt, *subVlist);
				break;
			}
			}
			break;
		}
		case 'd':
		case 'i':
		{
			intmax_t v = 0;
			switch (options.LengthMode)
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
			if (options.Precision == 0 && v == 0)
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
			if (options.Precision >= 0)
			{
				if (fieldLen < options.Precision)
				{
					zeroPadCount = options.Precision - fieldLen;
					fieldLen     = options.Precision;
				}
			}
			else if (options.ZeroPad &&
					 !options.LeftJustified &&
					 fieldLen < options.FieldWidth)
			{
				zeroPadCount = options.FieldWidth - fieldLen;
				fieldLen     = options.FieldWidth;
			}
			else if (fieldLen == 0)
			{
				zeroPadCount = 1;
				fieldLen     = 1;
			}

			switch (options.SignMode)
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
			}
			if (!WriteZeroPadding(stream, zeroPadCount, &len))
				goto BREAKOUT;
			if (stream)
			{
				ssize_t res = stream->WriteChars(stream, buf + start, 20 - start);
				if (res < 0)
					goto BREAKOUT;
				len += res;
				if (res < 20 - start)
					goto BREAKOUT;
			}
			else
			{
				len += 20 - start;
			}
			if (!RightSpacePad(stream, &options, fieldLen, &len))
				goto BREAKOUT;
			break;
		}
		case 'u':
		{
			uintmax_t v = 0;
			switch (options.LengthMode)
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
			if (options.Precision == 0 && v == 0)
				break;

			char   buf[21];
			size_t start = 20;
			buf[20]      = '\0';
			while (v)
			{
				intmax_t rem = v % 10;
				v           /= 10;
				buf[--start] = '0' + rem;
			}

			size_t fieldLen     = 20 - start;
			size_t zeroPadCount = 0;
			if (options.Precision >= 0)
			{
				if (fieldLen < options.Precision)
				{
					zeroPadCount = options.Precision - fieldLen;
					fieldLen     = options.Precision;
				}
			}
			else if (options.ZeroPad &&
					 !options.LeftJustified &&
					 fieldLen < options.FieldWidth)
			{
				zeroPadCount = options.FieldWidth - fieldLen;
				fieldLen     = options.FieldWidth;
			}
			else if (fieldLen == 0)
			{
				zeroPadCount = 1;
				fieldLen     = 1;
			}

			if (!LeftSpacePad(stream, &options, fieldLen, &len))
				goto BREAKOUT;
			if (!WriteZeroPadding(stream, zeroPadCount, &len))
				goto BREAKOUT;
			if (stream)
			{
				ssize_t res = stream->WriteChars(stream, buf + start, 20 - start);
				if (res < 0)
					goto BREAKOUT;
				len += res;
				if (res < 20 - start)
					goto BREAKOUT;
			}
			else
			{
				len += 20 - start;
			}
			if (!RightSpacePad(stream, &options, fieldLen, &len))
				goto BREAKOUT;
			break;
		}
		case 'o':
		{
			uintmax_t v = 0;
			switch (options.LengthMode)
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
			if (options.Precision == 0 && v == 0)
				break;

			char   buf[23];
			size_t start = 22;
			buf[22]      = '\0';
			while (v)
			{
				intmax_t rem = v & 7;
				v          >>= 3;
				buf[--start] = '0' + rem;
			}

			size_t fieldLen     = 22 - start;
			size_t zeroPadCount = 0;
			if (options.Precision >= 0)
			{
				if (fieldLen < options.Precision)
				{
					zeroPadCount = options.Precision - fieldLen;
					fieldLen     = options.Precision;
				}
			}
			else if (options.ZeroPad &&
					 !options.LeftJustified &&
					 fieldLen < options.FieldWidth)
			{
				zeroPadCount = options.FieldWidth - fieldLen;
				fieldLen     = options.FieldWidth;
			}
			else if (fieldLen == 0)
			{
				zeroPadCount = 1;
				fieldLen     = 1;
			}

			if (options.AltMode && zeroPadCount == 0)
			{
				++zeroPadCount;
				++fieldLen;
			}

			if (!LeftSpacePad(stream, &options, fieldLen, &len))
				goto BREAKOUT;
			if (!WriteZeroPadding(stream, zeroPadCount, &len))
				goto BREAKOUT;
			if (stream)
			{
				ssize_t res = stream->WriteChars(stream, buf + start, 22 - start);
				if (res < 0)
					goto BREAKOUT;
				len += res;
				if (res < 22 - start)
					goto BREAKOUT;
			}
			else
			{
				len += 22 - start;
			}
			if (!RightSpacePad(stream, &options, fieldLen, &len))
				goto BREAKOUT;
			break;
		}
		case 'x':
		case 'X':
		{
			bool      uppercase = options.FormatChar == 'X';
			uintmax_t v         = 0;
			switch (options.LengthMode)
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
			if (options.Precision == 0 && v == 0)
				break;

			char   buf[17];
			size_t start = 16;
			buf[16]      = '\0';
			while (v)
			{
				intmax_t rem = v & 0xF;
				v          >>= 4;
				buf[--start] = uppercase ? s_HexUppercase[rem] : s_HexLowercase[rem];
			}

			size_t fieldLen     = 22 - start;
			size_t zeroPadCount = 0;
			if (options.Precision >= 0)
			{
				if (fieldLen < options.Precision)
				{
					zeroPadCount = options.Precision - fieldLen;
					fieldLen     = options.Precision;
				}
			}
			else if (options.ZeroPad &&
					 !options.LeftJustified &&
					 fieldLen < options.FieldWidth)
			{
				zeroPadCount = options.FieldWidth - fieldLen;
				fieldLen     = options.FieldWidth;
			}
			else if (fieldLen == 0)
			{
				zeroPadCount = 1;
				fieldLen     = 1;
			}

			if (options.AltMode)
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
				case 2:
					zeroPadCount -= 2;
					break;
				}
			}

			if (!LeftSpacePad(stream, &options, fieldLen, &len))
				goto BREAKOUT;
			if (options.AltMode)
			{
				if (stream)
				{
					ssize_t res = stream->WriteChars(stream, uppercase ? "0X" : "0x", 2);
					if (res < 0)
						goto BREAKOUT;
					len += res;
					if (res < 2)
						goto BREAKOUT;
				}
				else
				{
					len += 2;
				}
			}
			if (!WriteZeroPadding(stream, zeroPadCount, &len))
				goto BREAKOUT;
			if (stream)
			{
				ssize_t res = stream->WriteChars(stream, buf + start, 16 - start);
				if (res < 0)
					goto BREAKOUT;
				len += res;
				if (res < 16 - start)
					goto BREAKOUT;
			}
			else
			{
				len += 16 - start;
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
			switch (options.LengthMode)
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
			switch (options.LengthMode)
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
			uintmax_t v = 0;
			switch (options.LengthMode)
			{
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8: v = (uintmax_t) va_arg(vlist, void*); break;
			}
			if (options.Precision == 0 && v == 0)
				break;

			char buf[17];
			buf[16] = '\0';
			for (size_t i = 0; i < 16; ++i)
			{
				intmax_t rem = v & 0xF;
				v          >>= 4;
				buf[15 - i]  = s_HexUppercase[rem];
			}

			if (!LeftSpacePad(stream, &options, 18, &len))
				goto BREAKOUT;
			if (stream)
			{
				ssize_t res = stream->WriteChars(stream, "0x", 2);
				if (res < 0)
					goto BREAKOUT;
				len += res;
				if (res < 2)
					goto BREAKOUT;
			}
			else
			{
				len += 2;
			}
			if (stream)
			{
				ssize_t res = stream->WriteChars(stream, buf, 16);
				if (res < 0)
					goto BREAKOUT;
				len += res;
				if (res < 16)
					goto BREAKOUT;
			}
			else
			{
				len += 16;
			}
			if (!RightSpacePad(stream, &options, 18, &len))
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