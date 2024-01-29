#include "MBR.hpp"
#include "PartitionSchemes/BootProtection.hpp"
#include "State.hpp"
#include <iomanip>
#include <iostream>

namespace MBR
{
	static uint8_t ParseHexDigit(char c)
	{
		switch (c)
		{
		case '0': return 0x0;
		case '1': return 0x1;
		case '2': return 0x2;
		case '3': return 0x3;
		case '4': return 0x4;
		case '5': return 0x5;
		case '6': return 0x6;
		case '7': return 0x7;
		case '8': return 0x8;
		case '9': return 0x9;
		case 'a':
		case 'A': return 0xA;
		case 'b':
		case 'B': return 0xB;
		case 'c':
		case 'C': return 0xC;
		case 'd':
		case 'D': return 0xD;
		case 'e':
		case 'E': return 0xE;
		case 'f':
		case 'F': return 0xF;
		default: return 0xFF;
		}
	}

	uint8_t ParsePartitionType(std::string_view arg)
	{
		if (CaselessStringCompare(arg, "fat") == 0)
			return 0x0C;
		else if (CaselessStringCompare(arg, "ef") == 0)
			return 0xEF;

		if (arg.size() == 1)
			return ParseHexDigit(arg[0]);
		else if (arg.size() == 2)
			return ParseHexDigit(arg[0]) << 4 | ParseHexDigit(arg[1]);
		else
			return 0x00;
	}

	void SetupPartitions(ImgGenOptions& options, std::fstream& fstream)
	{
		MBRHeader header;

		fstream.seekg(446);
		fstream.read((char*) &header, sizeof(header));
		if (options.Verbose && header.BootSignature != 0xAA55)
			std::cout << "ImgGen WARN: MBR Partition scheme contained invalid Boot Signature '" << std::hex << std::setw(4) << std::setfill('0') << header.BootSignature << std::dec << "', expected 'AA55', will fix on write\n";
		header.BootSignature = 0xAA55;
		bool forcePartitions = false;
		for (size_t i = 0; i < std::min<size_t>(4, options.PartitionCount); ++i)
		{
			auto& partition        = header.Partitions[i];
			auto& partitionOptions = options.PartitionOptions[i];
			if ((partition.StatusAndFirstSectorCHS & 0xFF) != 0x80)
			{
				std::cerr << "ImgGen ERROR: MBR partition '" << (i + 1) << "' is not valid, will reformat partition table\n";
				forcePartitions = true;
				break;
			}

			if ((partition.TypeAndLastSectorCHS & 0xFF) != partitionOptions.Type.iden)
			{
				std::cerr << "ImgGen ERROR: MBR partition '" << (i + 1) << "' is not the expected type '" << std::hex << std::setw(2) << std::setfill('0') << (uint16_t) partitionOptions.Type.iden << std::dec << "', will reformat partition if usable\n";
				partitionOptions.ForceReformat = true;
			}

			if (partition.LBACount < partitionOptions.ActualEnd - partitionOptions.ActualStart)
			{
				std::cerr << "ImgGen ERROR: MBR partition '" << (i + 1) << "' is smaller than what is requested '" << (partitionOptions.ActualEnd - partitionOptions.ActualStart) << "', will reformat partition table\n";
				forcePartitions = true;
				break;
			}
		}
		if (!forcePartitions)
		{
			for (size_t i = 0; i < std::min<size_t>(4, options.PartitionCount); ++i)
			{
				auto& partition              = header.Partitions[i];
				auto& partitionOptions       = options.PartitionOptions[i];
				partitionOptions.ActualStart = partition.FirstLBA;
				partitionOptions.ActualEnd   = partitionOptions.ActualStart + partition.LBACount;

				if (options.Verbose)
					std::cout << "ImgGen INFO: MBR partition '" << (i + 1) << "' declared with type '" << std::hex << std::setw(2) << std::setfill('0') << (partition.TypeAndLastSectorCHS & 0xFF) << std::dec << "', start '" << partition.FirstLBA << "', end '" << (partition.FirstLBA + partition.LBACount) << "'\n";
			}
		}
		else
		{
			for (size_t i = 0; i < 4; ++i)
				header.Partitions[i] = { .StatusAndFirstSectorCHS = 0, .TypeAndLastSectorCHS = 0, .FirstLBA = 0, .LBACount = 0 };
			for (size_t i = 0; i < std::min<size_t>(4, options.PartitionCount); ++i)
			{
				auto& partition               = header.Partitions[i];
				auto& partitionOption         = options.PartitionOptions[i];
				partitionOption.ForceReformat = true;

				partition.StatusAndFirstSectorCHS = GetCHS(partitionOption.ActualStart) << 8 | 0x80;
				partition.TypeAndLastSectorCHS    = GetCHS(partitionOption.ActualEnd) << 8 | partitionOption.Type.iden;
				partition.FirstLBA                = (uint32_t) partitionOption.ActualStart;
				partition.LBACount                = (uint32_t) (partitionOption.ActualEnd - partitionOption.ActualStart);
				if (options.Verbose)
					std::cout << "ImgGen INFO: MBR partition '" << (i + 1) << "' declared with type '" << std::hex << std::setw(2) << std::setfill('0') << (partition.TypeAndLastSectorCHS & 0xFF) << std::dec << "', start '" << partition.FirstLBA << "', end '" << (partition.FirstLBA + partition.LBACount) << "'\n";
			}
		}

		if (!forcePartitions && options.RetainBootCode)
		{
			fstream.seekp(446);
		}
		else
		{
			fstream.seekp(0);
			fstream.write((const char*) c_LegacyBootProtection, 446);
		}
		fstream.write((const char*) &header, sizeof(header));
	}

	uint64_t GetStartLBAOffset()
	{
		return 1;
	}

	uint64_t GetEndLBAOffset()
	{
		return 0;
	}

	uint64_t GetMinDriveSize()
	{
		return 0x100;
	}

	uint64_t GetMaxDriveSize()
	{
		return 0xFFFF'FFFF;
	}

	uint8_t GetMaxPartitionCount()
	{
		return 4;
	}

	uint32_t GetCHS(uint64_t lba)
	{
		uint64_t cylinder = lba + 1;
		uint64_t sector   = cylinder % 63;
		cylinder         /= 63;
		uint64_t head     = cylinder & 255;
		cylinder        >>= 8;
		if (cylinder >= 1023)
			return 0xFFFFFF;
		return ((uint32_t) (cylinder & 0xFF) << 16) | ((uint16_t) (sector | ((cylinder >> 8) & 0xC0)) << 8) | (uint8_t) head;
	}
} // namespace MBR