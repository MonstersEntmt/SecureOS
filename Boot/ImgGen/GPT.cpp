#include "GPT.h"
#include "CRC32.h"
#include "GUID.h"
#include "LegacyBootProtection.h"

#include <cstring>

#include <utility>

namespace GPT
{
	static constexpr std::pair<std::string_view, GUID> c_KnownPartitionTypes[] = {
		{ "EF00", { 0xC12A7328, 0xF81F, 0x11D2, 0xBA, 0x4B, { 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B } } },
		{ "MBR",  { 0x024DEE51, 0x33E7, 0x11D3, 0x9D, 0xD9, { 0x00, 0x08, 0xC7, 0x81, 0xF3, 0x9F } } }
	};

	static void WriteProtectiveMBR(GPTState& state, std::fstream& imageStream);
	static void WriteGPTHeader(const GPTHeader& header, std::fstream& imageStream);
	static void WriteGPTEntryArray(uint64_t firstLBA, const GPTPartitionEntry* entryArray, size_t entryCount, std::fstream& imageStream);

	GUID ParsePartitionType(std::string_view arg)
	{
		for (size_t i = 0; i < sizeof(c_KnownPartitionTypes) / sizeof(*c_KnownPartitionTypes); ++i)
		{
			auto& knownPartitionType = c_KnownPartitionTypes[i];
			if (arg == knownPartitionType.first)
				return knownPartitionType.second;
		}
		GUID guid {};
		if (!ParseGUID(arg, guid))
			return {};
		return guid;
	}

	void InitState(GPTState& state)
	{
		memcpy(state.Header.Signature, "EFI PART", 8);
		memcpy(state.Header.Revision, "\x00\x00\x01\x00", 4);
		state.Header.HeaderSize          = sizeof(GPTHeader);
		state.Header.Reserved            = 0;
		state.Header.PartitionEntrySize  = sizeof(GPTPartitionEntry);
		state.Header.HeaderCRC32         = 0;
		state.Header.CurrentLBA          = 1;
		state.Header.PartitionEntriesLBA = 2;
		state.Header.FirstUsableLBA      = 34;
		state.Header.LastUsableLBA       = state.Header.BackupLBA - 33;

		memset(state.Partitions, 0, sizeof(state.Partitions));
	}

	void WriteState(GPTState& state, std::fstream& imageStream)
	{
		GPTHeader backupHeader = state.Header;
		std::swap(backupHeader.CurrentLBA, backupHeader.BackupLBA);
		backupHeader.PartitionEntriesLBA = backupHeader.CurrentLBA - 32;

		state.Header.PartitionEntriesCRC32 = CRC32(state.Partitions, state.Header.PartitionEntryCount * sizeof(GPTPartitionEntry));
		state.Header.HeaderCRC32           = CRC32(&state.Header, sizeof(state.Header));
		backupHeader.PartitionEntriesCRC32 = state.Header.PartitionEntriesCRC32;
		backupHeader.HeaderCRC32           = CRC32(&backupHeader, sizeof(backupHeader));

		WriteProtectiveMBR(state, imageStream);
		WriteGPTHeader(state.Header, imageStream);
		WriteGPTHeader(backupHeader, imageStream);
		WriteGPTEntryArray(state.Header.PartitionEntriesLBA, state.Partitions, 128, imageStream);
		WriteGPTEntryArray(backupHeader.PartitionEntriesLBA, state.Partitions, 128, imageStream);
	}

	void WriteProtectiveMBR(GPTState& state, std::fstream& imageStream)
	{
		struct MBRPartitionRecord
		{
			uint8_t  BootIndicator;
			uint8_t  StartingCHS[3];
			uint8_t  OSType;
			uint8_t  EndingCHS[3];
			uint16_t StartingLBALow;
			uint16_t StartingLBAHigh;
			uint16_t SizeInLBALow;
			uint16_t SizeInLBAHigh;
		};
		struct ProtectiveMBR
		{
			uint32_t           Signature;
			uint16_t           Unknown;
			MBRPartitionRecord Partitions[4];
			uint16_t           BootSignature;
		};

		uint64_t cylinder = state.Header.BackupLBA + 1;
		uint64_t sector   = cylinder % 63;
		cylinder         /= 63;
		uint64_t head     = cylinder & 255;
		cylinder        >>= 8;
		if (cylinder > 1023)
		{
			cylinder = 1023;
			head     = 255;
			sector   = 63;
		}

		uint32_t size = (uint32_t) std::min<uint64_t>(state.Header.BackupLBA, 0xFFFF'FFFF);

		ProtectiveMBR header {
			.Signature  = 0,
			.Unknown    = 0,
			.Partitions = {
						   { .BootIndicator   = 0x00,
				  .StartingCHS     = { 0x00, 0x02, 0x00 },
				  .OSType          = 0xEE,
				  .EndingCHS       = { (uint8_t) head, (uint8_t) (sector | ((cylinder >> 8) & 0xC0)), (uint8_t) cylinder },
				  .StartingLBALow  = 1,
				  .StartingLBAHigh = 0,
				  .SizeInLBALow    = (uint16_t) size,
				  .SizeInLBAHigh   = (uint16_t) (size >> 16) },
						   {},
						   {},
						   {} },
			.BootSignature = 0xAA55
		};

		imageStream.seekp(0);
		imageStream.write((const char*) c_LegacyBootProtection, 440);
		imageStream.write((const char*) &header, sizeof(header));
	}

	void WriteGPTHeader(const GPTHeader& header, std::fstream& imageStream)
	{
		char buf[512];
		memset(buf, 0, 512);

		imageStream.seekp(header.CurrentLBA * 512);
		imageStream.write((const char*) &header, sizeof(GPTHeader));
		imageStream.write(buf, 512 - sizeof(GPTHeader));
	}

	void WriteGPTEntryArray(uint64_t firstLBA, const GPTPartitionEntry* entryArray, size_t entryCount, std::fstream& imageStream)
	{
		char buf[512];
		memset(buf, 0, 512);

		imageStream.seekp(firstLBA * 512);
		imageStream.write((const char*) entryArray, entryCount * sizeof(GPTPartitionEntry));
		if ((entryCount * sizeof(GPTPartitionEntry)) % 512 > 0)
			imageStream.write(buf, 512 - ((entryCount * sizeof(GPTPartitionEntry)) % 512));
	}
} // namespace GPT