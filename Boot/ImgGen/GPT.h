#pragma once

#include "GUID.h"

#include <fstream>

namespace GPT
{
	struct GPTPartitionEntry
	{
		GUID     TypeGUID;
		GUID     PartitionGUID;
		uint64_t FirstLBA;
		uint64_t LastLBA;
		uint64_t AttributeFlags;
		uint16_t Name[36];
	};

	struct GPTHeader
	{
		uint8_t  Signature[8];
		uint8_t  Revision[4];
		uint32_t HeaderSize;
		uint32_t HeaderCRC32;
		uint32_t Reserved;
		uint64_t CurrentLBA;
		uint64_t BackupLBA;
		uint64_t FirstUsableLBA;
		uint64_t LastUsableLBA;
		GUID     DiskGUID;
		uint64_t PartitionEntriesLBA;
		uint32_t PartitionEntryCount;
		uint32_t PartitionEntrySize;
		uint32_t PartitionEntriesCRC32;
	};

	struct GPTState
	{
		GPTHeader         Header;
		GPTPartitionEntry Partitions[128];
	};

	GUID ParsePartitionType(std::string_view arg);

	void InitState(GPTState& state);

	void WriteState(GPTState& state, std::fstream& imageStream);
} // namespace GPT