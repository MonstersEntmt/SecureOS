#pragma once

#include "State.hpp"

#include <cstdint>
#include <fstream>
#include <string_view>

namespace MBR
{
	struct MBRPartition
	{
		uint32_t StatusAndFirstSectorCHS;
		uint32_t TypeAndLastSectorCHS;
		uint32_t FirstLBA;
		uint32_t LBACount;
	};

	struct MBRHeader
	{
		MBRPartition Partitions[4];
		uint16_t     BootSignature;
	};

	uint8_t ParsePartitionType(std::string_view arg);

	void SetupPartitions(ImgGenOptions& options, std::fstream& fstream);

	uint64_t GetStartLBAOffset();
	uint64_t GetEndLBAOffset();
	uint64_t GetMinDriveSize();
	uint64_t GetMaxDriveSize();
	uint8_t  GetMaxPartitionCount();

	uint32_t GetCHS(uint64_t lba);
} // namespace MBR