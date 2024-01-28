#pragma once

#include "GUID.hpp"
#include "State.hpp"

#include <fstream>
#include <string_view>

namespace GPT
{
	struct GPTPartition
	{
		GUID     Type;
		GUID     ID;
		uint64_t FirstLBA;
		uint64_t LastLBA;
		uint64_t Attribute;
		uint16_t Name[36];
	};

	struct GPTHeader
	{
		uint64_t Signature;
		uint32_t Revision;
		uint32_t HeaderSize;
		uint32_t CRC32;
		uint32_t Reserved;
		uint64_t CurrentLBA;
		uint64_t BackupLBA;
		uint64_t FirstUsableLBA;
		uint64_t LastUsableLBA;
		GUID     DiskGUID;
		uint64_t PartitionsLBA;
		uint32_t PartitionCount;
		uint32_t PartitionSize;
		uint32_t PartitionCRC32;
	};

	GUID ParsePartitionType(std::string_view arg);

	void SetupPartitions(ImgGenOptions& options, std::fstream& fstream);

	uint64_t GetStartLBAOffset();
	uint64_t GetEndLBAOffset();
	uint64_t GetMinDriveSize();
	uint64_t GetMaxDriveSize();
	uint8_t  GetMaxPartitionCount();
} // namespace GPT