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
		uint64_t mbrSectorCount = std::min<uint64_t>(state.Header.BackupLBA + 1, 0xFFFF'FFFE);
		uint64_t partitionRecord[2];
		partitionRecord[0] = 0xFFFE'FEEE'0001'0080;
		partitionRecord[1] = 0x0000'0000'0000'0001 | (mbrSectorCount << 32);

		imageStream.seekp(0);
		imageStream.write((const char*) c_LegacyBootProtection, 440);
		imageStream.write("\x00\x00\x00\x00", 4);
		imageStream.write("\x00\x00", 2);
		imageStream.write((const char*) &partitionRecord, 16);
		partitionRecord[0] = 0;
		partitionRecord[1] = 0;
		imageStream.write((const char*) &partitionRecord, 16);
		imageStream.write((const char*) &partitionRecord, 16);
		imageStream.write((const char*) &partitionRecord, 16);
		imageStream.write("\x55\xAA", 2);
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