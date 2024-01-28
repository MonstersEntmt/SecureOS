#include "Helpers.hpp"
#include "PartitionSchemes/GPT.hpp"
#include "PartitionSchemes/MBR.hpp"
#include "PartitionTypes/FAT.hpp"
#include "State.hpp"
#include <iomanip>
#include <iostream>

namespace PartHelpers
{
	void ValidateOptions(ImgGenOptions& options)
	{
		switch (options.PartitionScheme)
		{
		case EPartitionScheme::MBR:
			if (options.Verbose)
				std::cout << "ImgGen INFO: will produce an MBR partitioned image\n";
			break;
		case EPartitionScheme::GPT:
			if (options.Verbose)
				std::cout << "ImgGen INFO: will produce an GPT partitioned image\n";
			break;
		}
	}

	void PartitionRanges(ImgGenOptions& options)
	{
		size_t   partitionAlignment = std::max<uint64_t>(options.PhysicalSize, options.TransferGranularity) / 512;
		uint64_t currentStart       = 0;
		uint64_t currentEnd         = 0;
		switch (options.PartitionScheme)
		{
		case EPartitionScheme::MBR:
			currentStart = MBR::GetStartLBAOffset();
			currentEnd   = MBR::GetEndLBAOffset();

			if (options.PartitionCount > MBR::GetMaxPartitionCount())
			{
				std::cerr << "ImgGen ERROR: MBR partition scheme only allows " << (uint16_t) MBR::GetMaxPartitionCount() << " partitions, will skip the rest\n";
				options.PartitionCount = MBR::GetMaxPartitionCount();
			}
			break;
		case EPartitionScheme::GPT:
			currentStart = GPT::GetStartLBAOffset();
			currentEnd   = GPT::GetEndLBAOffset();

			if (options.PartitionCount > GPT::GetMaxPartitionCount())
			{
				std::cerr << "ImgGen ERROR: GPT partition scheme only allows " << (uint16_t) GPT::GetMaxPartitionCount() << " partitions, will skip the rest\n";
				options.PartitionCount = GPT::GetMaxPartitionCount();
			}
			break;
		}
		currentStart = (currentStart + partitionAlignment - 1) / partitionAlignment * partitionAlignment;
		currentEnd   = (currentEnd + partitionAlignment - 1) / partitionAlignment * partitionAlignment;

		size_t endBegin = options.PartitionCount;
		for (size_t i = 0; i < options.PartitionCount; ++i)
		{
			auto& partition = options.PartitionOptions[i];
			if (endBegin == options.PartitionCount && (partition.StartMode == ESectorMode::EndRelative || partition.EndMode == ESectorMode::EndRelative))
				endBegin = i;

			if (endBegin != options.PartitionCount)
			{
				switch (partition.StartMode)
				{
				case ESectorMode::Relative:
					if (i != endBegin)
					{
						std::cerr << "ImgGen ERROR: partition '" << (i + 1) << "' starts with an unexpected start relative sector\n";
						exit(1);
					}
					break;
				case ESectorMode::EndRelative: break;
				case ESectorMode::Absolute:
					if (i != endBegin)
					{
						std::cerr << "ImgGen ERROR: partition '" << (i + 1) << "' starts with an unexpected absolute sector\n";
						exit(1);
					}
					break;
				}
				switch (partition.EndMode)
				{
				case ESectorMode::Relative:
					std::cerr << "ImgGen ERROR: partition '" << (i + 1) << "' ends with an unexpected start relative sector\n";
					exit(1);
					break;
				case ESectorMode::EndRelative: break;
				case ESectorMode::Absolute:
					std::cerr << "ImgGen ERROR: partition '" << (i + 1) << "' ends with an unexpected absolute sector\n";
					exit(1);
					break;
				}
			}
			else
			{
				if (partition.StartMode == ESectorMode::EndRelative)
				{
					std::cerr << "ImgGen ERROR: partition '" << (i + 1) << "' starts with an unexpected end relative sector\n";
					exit(1);
				}
				if (partition.EndMode == ESectorMode::EndRelative)
				{
					std::cerr << "ImgGen ERROR: partition '" << (i + 1) << "' ends with an unexpected end relative sector\n";
					exit(1);
				}
			}
		}

		for (size_t i = 0; i < endBegin; ++i)
		{
			auto& partition = options.PartitionOptions[i];
			switch (partition.StartMode)
			{
			case ESectorMode::Relative: partition.ActualStart = (currentStart + partition.Start + partitionAlignment - 1) / partitionAlignment * partitionAlignment; break;
			case ESectorMode::EndRelative: // This should never hit, unless some UB has happened
				std::cerr << "ImgGen UB: partition " << (i + 1) << " got through relative start validation, please send reproducable copy of the command used\n";
				exit(1);
				break;
			case ESectorMode::Absolute:
				if (partition.Start < currentStart)
				{
					std::cerr << "ImgGen ERROR: partition '" << (i + 1) << "' absolute start '" << partition.Start << "' is below the current start '" << currentStart << "'\n";
					exit(1);
				}
				partition.ActualStart = partition.Start;
				break;
			}
			currentStart = partition.ActualStart;

			switch (partition.EndMode)
			{
			case ESectorMode::Relative: partition.ActualEnd = (currentStart + partition.End + partitionAlignment - 1) / partitionAlignment * partitionAlignment; break;
			case ESectorMode::EndRelative: // This should never hit, unless some UB has happened
				std::cerr << "ImgGen UB: partition " << (i + 1) << " got through relative end validation, please send reproducable copy of the command used\n";
				exit(1);
				break;
			case ESectorMode::Absolute:
				if (partition.End < currentStart)
				{
					std::cerr << "ImgGen ERROR: partition '" << (i + 1) << "' absolute end '" << partition.End << "' is below the current start '" << currentStart << "'\n";
					exit(1);
				}
				partition.ActualEnd = partition.End;
				break;
			}
			switch (partition.Format)
			{
			case EPartitionFormat::None: break;
			case EPartitionFormat::FAT:
				if (partition.ActualEnd - partition.ActualStart < FAT::GetMinPartitionSize())
				{
					std::cerr << "ImgGen WARN: partition '" << (i + 1) << "' is too small to fit a FAT filesystem '" << (partition.ActualEnd - partition.ActualStart) << "' vs '" << FAT::GetMinPartitionSize() << "', will expand\n";
					partition.ActualEnd = (partition.ActualStart + FAT::GetMinPartitionSize() + partitionAlignment - 1) / partitionAlignment * partitionAlignment;
				}
				break;
			}
			currentStart = partition.ActualEnd;
		}
		for (size_t i = options.PartitionCount; --i > endBegin;)
		{
			auto& partition = options.PartitionOptions[i];
			switch (partition.EndMode)
			{
			case ESectorMode::Relative: // This should never hit, unless some UB has happened
				std::cerr << "ImgGen UB: partition " << (i + 1) << " got through relative end validation, please send reproducable copy of the command used\n";
				exit(1);
				break;
			case ESectorMode::EndRelative: partition.ActualEnd = (currentEnd + partition.End + partitionAlignment - 1) / partitionAlignment * partitionAlignment; break;
			case ESectorMode::Absolute: // This should never hit, unless some UB has happened
				std::cerr << "ImgGen UB: partition " << (i + 1) << " got through absolute validation, please send reproducable copy of the command used\n";
				exit(1);
				break;
			}
			currentEnd = partition.ActualEnd;

			switch (partition.StartMode)
			{
			case ESectorMode::Relative: // This should never hit, unless some UB has happened
				std::cerr << "ImgGen UB: partition " << (i + 1) << " got through relative start validation, please send reproducable copy of the command used\n";
				exit(1);
				break;
			case ESectorMode::EndRelative: partition.ActualStart = (currentEnd + partition.Start + partitionAlignment - 1) / partitionAlignment * partitionAlignment; break;
			case ESectorMode::Absolute: // This should never hit, unless some UB has happened
				std::cerr << "ImgGen UB: partition " << (i + 1) << " got through absolute validation, please send reproducable copy of the command used\n";
				exit(1);
				break;
			}
			switch (partition.Format)
			{
			case EPartitionFormat::None: break;
			case EPartitionFormat::FAT:
				if (partition.ActualEnd - partition.ActualStart < FAT::GetMinPartitionSize())
				{
					std::cerr << "ImgGen WARN: partition '" << (i + 1) << "' is too small to fit a FAT filesystem '" << (partition.ActualEnd - partition.ActualStart) << "' vs '" << FAT::GetMinPartitionSize() << "', will expand\n";
					partition.ActualStart = (partition.ActualEnd + FAT::GetMinPartitionSize() + partitionAlignment - 1) / partitionAlignment * partitionAlignment;
				}
				break;
			}
			currentEnd = partition.ActualStart;
		}
		if (endBegin != options.PartitionCount)
		{
			auto& partition = options.PartitionOptions[endBegin];
			switch (partition.StartMode)
			{
			case ESectorMode::Relative:
				partition.ActualStart = (currentStart + partition.Start + partitionAlignment - 1) / partitionAlignment * partitionAlignment;
				currentStart          = partition.ActualStart;
				switch (partition.EndMode)
				{
				case ESectorMode::Relative: // This should never hit, unless some UB has happened
					std::cerr << "ImgGen UB: partition " << endBegin << " got through mid validation, please send reproducable copy of the command used\n";
					exit(1);
					break;
				case ESectorMode::EndRelative:
					partition.ActualEnd = (currentEnd + partition.End + partitionAlignment - 1) / partitionAlignment * partitionAlignment;
					currentEnd          = partition.ActualEnd;
					break;
				case ESectorMode::Absolute: // This should never hit, unless some UB has happened
					std::cerr << "ImgGen UB: partition " << endBegin << " got through mid validation, please send reproducable copy of the command used\n";
					exit(1);
					break;
				}
				break;
			case ESectorMode::EndRelative:
				switch (partition.EndMode)
				{
				case ESectorMode::Relative: // This should never hit, unless some UB has happened
					std::cerr << "ImgGen UB: partition " << endBegin << " got through end relative validation, please send reproducable copy of the command used\n";
					exit(1);
					break;
				case ESectorMode::EndRelative:
					partition.ActualEnd = (currentEnd + partition.End + partitionAlignment - 1) / partitionAlignment + partitionAlignment;
					currentEnd          = partition.ActualEnd;
					break;
				case ESectorMode::Absolute: // This should never hit, unless some UB has happened
					std::cerr << "ImgGen UB: partition " << endBegin << " got through absolute validation, please send reproducable copy of the command used\n";
					exit(1);
					break;
				}
				partition.ActualStart = (currentStart + partition.Start + partitionAlignment - 1) / partitionAlignment + partitionAlignment;
				currentStart          = partition.ActualStart;
				break;
			case ESectorMode::Absolute:
				if (partition.Start < currentStart)
				{
					std::cerr << "ImgGen ERROR: partition '" << endBegin << "' absolute start '" << partition.Start << "' is below the current start '" << currentStart << "'\n";
					exit(1);
				}
				partition.ActualStart = partition.Start;
				currentStart          = partition.ActualStart;
				switch (partition.EndMode)
				{
				case ESectorMode::Relative: // This should never hit, unless some UB has happened
					std::cerr << "ImgGen UB: partition " << endBegin << " got through mid validation, please send reproducable copy of the command used\n";
					exit(1);
					break;
				case ESectorMode::EndRelative:
					partition.ActualEnd = (currentEnd + partition.End + partitionAlignment - 1) / partitionAlignment + partitionAlignment;
					currentEnd          = partition.ActualEnd;
					break;
				case ESectorMode::Absolute: // This should never hit, unless some UB has happened
					std::cerr << "ImgGen UB: partition " << endBegin << " got through mid validation, please send reproducable copy of the command used\n";
					exit(1);
					break;
				}
				break;
			}
		}
		size_t requiredImageSize = (currentStart + currentEnd) * 512;
		currentEnd              += ((options.ImageSize + 511) / 512) - currentStart - currentEnd;
		if (endBegin != options.PartitionCount)
		{
			auto&  partition      = options.PartitionOptions[endBegin];
			size_t partitionStart = 0;
			size_t partitionEnd   = 0;
			switch (partition.StartMode)
			{
			case ESectorMode::Relative:
				partitionStart = partition.ActualStart;
				switch (partition.EndMode)
				{
				case ESectorMode::Relative:
				case ESectorMode::Absolute: partitionEnd = partition.ActualEnd; break;
				case ESectorMode::EndRelative:
					partitionEnd = partitionStart + (currentEnd - partition.ActualEnd);
					break;
				}
				break;
			case ESectorMode::EndRelative:
				partitionStart = currentStart + (currentEnd - partition.ActualStart);
				switch (partition.EndMode)
				{
				case ESectorMode::Relative: partitionEnd = partition.ActualEnd; break;
				case ESectorMode::EndRelative:
				case ESectorMode::Absolute:
					partitionEnd = partitionStart + (currentEnd - partition.ActualEnd);
					break;
				}
				break;
			case ESectorMode::Absolute:
				partitionStart = partition.ActualStart;
				switch (partition.EndMode)
				{
				case ESectorMode::Relative:
				case ESectorMode::Absolute: partitionEnd = partition.End; break;
				case ESectorMode::EndRelative:
					partitionEnd = partitionStart + (currentEnd - partition.ActualEnd);
					break;
				}
				break;
			}
			switch (partition.Format)
			{
			case EPartitionFormat::None: break;
			case EPartitionFormat::FAT:
				if (partitionEnd - partitionStart < FAT::GetMinPartitionSize())
				{
					std::cerr << "ImgGen WARN: partition '" << endBegin << "' is too small to fit a FAT filesystem '" << (partitionEnd - partitionStart) << "' vs '" << FAT::GetMinPartitionSize() << "', will expand\n";
					requiredImageSize += (FAT::GetMinPartitionSize() + partitionAlignment - 1) / partitionAlignment * partitionAlignment - (partitionEnd - partitionStart);
				}
				break;
			}
		}

		if (options.CanExpand)
		{
			if (options.ImageSize > 0 && options.ImageSize < requiredImageSize)
			{
				std::cout << "ImgGen WARN: image size '" << options.ImageSize << "' is too small to fit the required partitions '" << requiredImageSize << "', will expand\n";
				options.ImageSize = requiredImageSize;
			}
		}
		else if (options.ImageSize < requiredImageSize)
		{
			std::cerr << "ImgGen ERROR: image size '" << options.ImageSize << "' is too small to fit the required partitions '" << requiredImageSize << "'\n";
			exit(1);
		}

		for (size_t i = endBegin; i < options.PartitionCount; ++i)
		{
			auto& partition = options.PartitionOptions[i];
			switch (partition.StartMode)
			{
			case ESectorMode::Relative:
				switch (partition.EndMode)
				{
				case ESectorMode::Relative: break;
				case ESectorMode::EndRelative:
					partition.ActualEnd = currentStart + (currentEnd - partition.ActualEnd);
					break;
				case ESectorMode::Absolute: break;
				}
				break;
			case ESectorMode::EndRelative:
				partition.ActualStart = currentStart + (currentEnd - partition.ActualStart);
				switch (partition.EndMode)
				{
				case ESectorMode::Relative: break;
				case ESectorMode::EndRelative:
				case ESectorMode::Absolute:
					partition.ActualEnd = currentStart + (currentEnd - partition.ActualEnd);
					break;
				}
				break;
			case ESectorMode::Absolute:
				switch (partition.EndMode)
				{
				case ESectorMode::Relative: break;
				case ESectorMode::EndRelative:
					partition.ActualEnd = currentStart + (currentEnd - partition.ActualEnd);
					break;
				case ESectorMode::Absolute: break;
				}
				break;
			}
		}

		if (options.Verbose)
		{
			for (size_t i = 0; i < options.PartitionCount; ++i)
			{
				auto& partition = options.PartitionOptions[i];
				std::cout << "ImgGen INFO: partition '" << (i + 1) << "' starts at sector '" << partition.ActualStart << "' and ends at sector '" << partition.ActualEnd << "'\n";
				switch (options.PartitionScheme)
				{
				case EPartitionScheme::MBR: std::cout << "  Type '" << std::hex << std::setw(2) << std::setfill('0') << (uint16_t) partition.Type.iden << std::dec << "'\n"; break;
				case EPartitionScheme::GPT: std::cout << "  Type '" << partition.Type.guid << "'\n"; break;
				}
				switch (partition.Format)
				{
				case EPartitionFormat::None: std::cout << "  Unformatted\n"; break;
				case EPartitionFormat::FAT: std::cout << "  Formatted as FAT\n"; break;
				}
			}
		}

		switch (options.PartitionScheme)
		{
		case EPartitionScheme::MBR:
			if (options.ImageSize < MBR::GetMinDriveSize())
			{
				std::cerr << "ImgGen " << (options.CanExpand ? "WARN" : "ERROR") << ": MBR partition scheme requires at least " << MBR::GetMinDriveSize() << " bytes to work";
				if (!options.CanExpand)
				{
					std::cerr << '\n';
					exit(1);
				}

				std::cerr << ", will grow image size\n";
				options.ImageSize = MBR::GetMinDriveSize();
			}
			else if (options.ImageSize > MBR::GetMaxDriveSize())
			{
				std::cerr << "ImgGen ERROR: MBR partition scheme only allows up to " << MBR::GetMaxDriveSize() << " bytes\n";
				exit(1);
			}
			break;
		case EPartitionScheme::GPT:
			if (options.ImageSize < GPT::GetMinDriveSize())
			{
				std::cerr << "ImgGen " << (options.CanExpand ? "WARN" : "ERROR") << ": GPT partition scheme requires at least " << GPT::GetMinDriveSize() << " bytes to work";
				if (!options.CanExpand)
				{
					std::cerr << '\n';
					exit(1);
				}

				std::cerr << ", will grow image size\n";
				options.ImageSize = GPT::GetMinDriveSize();
			}
			else if (options.ImageSize > GPT::GetMaxDriveSize())
			{
				std::cerr << "ImgGen ERROR: GPT partition scheme only allows up to " << GPT::GetMaxDriveSize() << " bytes\n";
				exit(1);
			}
			break;
		}
	}

	void ReadSector(PartitionOptions& partition, std::fstream& fstream, void* buffer, uint64_t sector)
	{
		fstream.seekg((partition.ActualStart + sector) * 512);
		fstream.read((char*) buffer, 512);
	}

	void WriteSector(PartitionOptions& partition, std::fstream& fstream, const void* buffer, uint64_t sector)
	{
		fstream.seekp((partition.ActualStart + sector) * 512);
		fstream.write((const char*) buffer, 512);
	}
} // namespace PartHelpers