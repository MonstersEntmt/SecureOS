#pragma once

#include "State.hpp"
#include <fstream>

namespace PartHelpers
{
	void ValidateOptions(ImgGenOptions& options);
	void PartitionRanges(ImgGenOptions& options);

	void ReadSector(PartitionOptions& partition, std::fstream& fstream, void* buffer, uint64_t sector);
	void WriteSector(PartitionOptions& partition, std::fstream& fstream, const void* buffer, uint64_t sector);
} // namespace PartHelpers