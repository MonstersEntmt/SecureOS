#include "GPT.hpp"
#include "CRC32.hpp"
#include "GUID.hpp"
#include "PartitionSchemes/BootProtection.hpp"
#include "PartitionSchemes/MBR.hpp"
#include "Utils/UTF.hpp"

#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string_view>

namespace GPT
{
	static constexpr std::pair<std::string_view, GUID> c_KnownPartitionTypes[] = {
		{ "EF00", { 0xC12A7328, 0xF81F, 0x11D2, 0xBA, 0x4B, { 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B } } },
		{ "MBR",  { 0x024DEE51, 0x33E7, 0x11D3, 0x9D, 0xD9, { 0x00, 0x08, 0xC7, 0x81, 0xF3, 0x9F } } }
	};

	GUID ParsePartitionType(std::string_view arg)
	{
		for (auto& type : c_KnownPartitionTypes)
		{
			if (type.first == arg)
				return type.second;
		}
		GUID guid {};
		if (!ParseGUID(arg, guid))
			return {};
		return guid;
	}

	void SetupPartitions(ImgGenOptions& options, std::fstream& fstream)
	{
		MBR::MBRHeader mbrHeader;
		GPTHeader      header;
		GPTHeader      backupHeader;
		GPTPartition   partitions[128];
		memset(partitions, 0, 128 * sizeof(GPTPartition));

		uint64_t firstLBA  = GetStartLBAOffset();
		uint64_t lastLBA   = (options.ImageSize + 511) / 512 - GetEndLBAOffset();
		uint64_t backupLBA = (options.ImageSize + 511) / 512 - 1;

		fstream.seekg(446);
		fstream.read((char*) &mbrHeader, sizeof(mbrHeader));
		if (options.Verbose && mbrHeader.BootSignature != 0xAA55)
			std::cout << "ImgGen WARN: MBR contained invalid Boot Signature '" << std::hex << std::setw(4) << std::setfill('0') << mbrHeader.BootSignature << std::dec << "', expected 'AA55', will fix on write\n";
		mbrHeader.BootSignature = 0xAA55;
		int state               = 0;
		if ((mbrHeader.Partitions[0].TypeAndLastSectorCHS & 0xFF) != 0xEE)
		{
			if (options.Verbose)
				std::cout << "ImgGen WARN: MBR Partitioned drive will be reformatted to a GPT Partitioned drive\n";
			for (size_t i = 0; i < options.PartitionCount; ++i)
				options.PartitionOptions[i].ForceReformat = true;
			state = 3;

			mbrHeader.Partitions[0] = {
				.StatusAndFirstSectorCHS = 0x00020000,
				.TypeAndLastSectorCHS    = (MBR::GetCHS(backupLBA) << 8) | 0xEE,
				.FirstLBA                = 1,
				.LBACount                = (uint32_t) std::min<size_t>(backupLBA, 0xFFFF'FFFF)
			};
			for (size_t i = 1; i < 4; ++i)
				mbrHeader.Partitions[i] = {};
		}
		else
		{
			auto& protectivePartition = mbrHeader.Partitions[0];
			if (protectivePartition.StatusAndFirstSectorCHS != 0x00020000)
			{
				if (options.Verbose)
					std::cout << "ImgGen WARN: Protective MBR partition contains invalid status or first sector CHS, will fix on write\n";
				protectivePartition.StatusAndFirstSectorCHS = 0x00020000;
			}
			uint32_t lastCHS = MBR::GetCHS(backupLBA);
			if ((protectivePartition.TypeAndLastSectorCHS >> 8) != lastCHS)
			{
				if (options.Verbose)
					std::cout << "ImgGen WARN: Protective MBR partition contains invalid last sector CHS, will fix on write\n";
				std::cout << (protectivePartition.TypeAndLastSectorCHS >> 8) << " vs " << lastCHS << '\n';
				protectivePartition.TypeAndLastSectorCHS = (lastCHS << 8) | 0xEE;
			}
			if (protectivePartition.FirstLBA != 1)
			{
				if (options.Verbose)
					std::cout << "ImgGen WARN: Protective MBR partition contains invalid first LBA, will fix on write\n";
				protectivePartition.FirstLBA = 1;
			}
			if (protectivePartition.LBACount != std::min<size_t>(backupLBA, 0xFFFF'FFFF))
			{
				if (options.Verbose)
					std::cout << "ImgGen WARN: Protective MBR partition contains invalid LBA count, will fix on write\n";
				protectivePartition.LBACount = std::min<size_t>(backupLBA, 0xFFFF'FFFF);
			}
			for (size_t i = 1; i < 4; ++i)
			{
				auto& partition = mbrHeader.Partitions[i];
				if (options.Verbose && (partition.StatusAndFirstSectorCHS != 0 || partition.TypeAndLastSectorCHS != 0 || partition.FirstLBA != 0 || partition.LBACount != 0))
					std::cout << "ImgGen WARN: Protective MBR has non invalid partition, will remove on write\n";
				partition.StatusAndFirstSectorCHS = 0;
				partition.TypeAndLastSectorCHS    = 0;
				partition.FirstLBA                = 0;
				partition.LBACount                = 0;
			}
		}

		if (state != 2 && options.RetainBootCode)
		{
			fstream.seekp(446);
			std::cout << "ImgGen WARN: will attempt to retain bootcode, could potentially break bootcode\n";
		}
		else
		{
			fstream.seekp(0);
			fstream.write((const char*) c_LegacyBootProtection, 446);
		}
		fstream.write((const char*) &mbrHeader, sizeof(mbrHeader));

		if (state != 3)
		{
			do
			{
				fstream.seekg(512);
				fstream.read((char*) &header, sizeof(GPTHeader));
				if (header.Signature != 0x5452415020494645ULL)
				{
					std::cerr << "ImgGen ERROR: partition scheme is not a valid GPT partition scheme, will use backup if present and valid\n";
					break;
				}

				uint32_t crc32      = header.CRC32;
				header.CRC32        = 0;
				uint32_t foundCRC32 = CRC32(&header, sizeof(GPTHeader));
				if (crc32 != foundCRC32)
				{
					std::cout << "ImgGen ERROR: GPT header contains invalid CRC32 '" << std::hex << std::setw(8) << std::setfill('0') << crc32 << std::dec << "', expected '" << std::hex << std::setw(8) << std::setfill('0') << foundCRC32 << std::dec << "', will use backup if present and valid\n";
					break;
				}

				if (header.CurrentLBA != 1)
				{
					std::cerr << "ImgGen ERROR: main GPT header does not point to itself, will use backup if present and valid\n";
					break;
				}
				backupLBA = header.BackupLBA;

				fstream.seekg(512 * header.PartitionsLBA);
				fstream.read((char*) partitions, 128 * sizeof(GPTPartition));

				foundCRC32 = CRC32(partitions, header.PartitionCount * sizeof(GPTPartition));
				if (header.PartitionCRC32 != foundCRC32)
				{
					if (options.Verbose)
						std::cout << "ImgGen ERROR: GPT header contains invalid partitions CRC32 '" << std::hex << std::setw(8) << std::setfill('0') << header.PartitionCRC32 << std::dec << "', expected '" << std::hex << std::setw(8) << std::setfill('0') << foundCRC32 << std::dec << "', will use backup if present and valid\n";
					break;
				}
				state = 1;
			}
			while (false);
		}
		if (state == 0)
		{
			size_t i = (backupLBA == (options.ImageSize + 511) / 512 - 1) ? 1 : 0; // Ensures we only test once if the primary header had invalid signature or CRC32
			for (; i < 2; ++i, backupLBA = (options.ImageSize + 511) / 512 - 1)
			{
				if (options.Verbose)
					std::cout << "ImgGen INFO: attempting to recover backup header at '" << backupLBA << "'\n";

				fstream.seekg(backupLBA * 512);
				fstream.read((char*) &backupHeader, sizeof(GPTHeader));
				if (backupHeader.Signature != 0x5452415020494645ULL)
				{
					std::cerr << "ImgGen ERROR: partition scheme is not a valid GPT partition scheme, will reformat\n";
					continue;
				}

				uint32_t crc32      = backupHeader.CRC32;
				backupHeader.CRC32  = 0;
				uint32_t foundCRC32 = CRC32(&backupHeader, sizeof(GPTHeader));
				if (crc32 != foundCRC32)
				{
					std::cout << "ImgGen ERROR: GPT backup header contains invalid CRC32 '" << std::hex << std::setw(8) << std::setfill('0') << crc32 << std::dec << "', expected '" << std::hex << std::setw(8) << std::setfill('0') << foundCRC32 << std::dec << "', will reformat\n";
					continue;
				}

				if (backupHeader.CurrentLBA != backupLBA)
				{
					std::cerr << "ImgGen ERROR: backup GPT header does not point to itself, will reformat\n";
					continue;
				}

				fstream.seekg(512 * backupHeader.PartitionsLBA);
				fstream.read((char*) partitions, 128 * sizeof(GPTPartition));

				foundCRC32 = CRC32(partitions, backupHeader.PartitionCount * sizeof(GPTPartition));
				if (backupHeader.PartitionCRC32 != foundCRC32)
				{
					if (options.Verbose)
						std::cout << "ImgGen ERROR: GPT header contains invalid partitions CRC32 '" << std::hex << std::setw(8) << std::setfill('0') << header.PartitionCRC32 << std::dec << "', expected '" << std::hex << std::setw(8) << std::setfill('0') << foundCRC32 << std::dec << "', will use backup if present and valid\n";
					continue;
				}
				break;
			}
			state = (i == 2) ? 3 : 2;
		}

		switch (state)
		{
		case 0: // This should never hit, unless some UB happened
			std::cerr << "ImgGen UB: GPT state 0, please send reproducable copy of the command used\n";
			exit(1);
			break;
		case 1: // Primary header will be used
			break;
		case 2: // Backup header will be used
			memcpy(&header, &backupHeader, sizeof(GPTHeader));
			header.CRC32      = 0;
			header.CurrentLBA = 1;
			header.BackupLBA  = backupHeader.CurrentLBA;
			break;
		case 3: // Reformat header
			memset(&header, 0, sizeof(GPTHeader));

			header.Signature      = 0x5452415020494645ULL;
			header.Revision       = 0x00010000;
			header.HeaderSize     = sizeof(GPTHeader);
			header.CurrentLBA     = 1;
			header.BackupLBA      = backupLBA;
			header.FirstUsableLBA = firstLBA;
			header.LastUsableLBA  = lastLBA;
			header.DiskGUID       = RandomGUID();
			header.PartitionsLBA  = 2;
			header.PartitionCount = 0;
			header.PartitionSize  = 128;
			break;
		}

		if (state != 3 && header.PartitionCount < options.PartitionCount)
		{
			std::cout << "ImgGen WARN: GPT partition table does not contain enough partitions, will reformat partition table\n";
			state = 3;
		}

		if (state != 3)
		{
			for (size_t i = 0; i < options.PartitionCount; ++i)
			{
				auto& partition        = partitions[i];
				auto& partitionOptions = options.PartitionOptions[i];

				if (partition.Type != partitionOptions.Type.guid)
				{
					std::cerr << "ImgGen ERROR: GPT partition '" << (i + 1) << "' is not the expected type '" << partitionOptions.Type.guid << "', will reformat partition if usable\n";
					partitionOptions.ForceReformat = true;
				}

				if (partition.LastLBA - partition.FirstLBA + 1 < partitionOptions.ActualEnd - partitionOptions.ActualStart)
				{
					std::cerr << "ImgGen ERROR: GPT partition '" << (i + 1) << "' is smaller than what is requested '" << (partitionOptions.ActualEnd - partitionOptions.ActualStart) << "', will reformat partition table\n";
					state = 3;
					break;
				}
			}
		}

		if (state != 3)
		{
			for (size_t i = 0; i < options.PartitionCount; ++i)
			{
				auto& partition        = partitions[i];
				auto& partitionOptions = options.PartitionOptions[i];

				partitionOptions.ActualStart = partition.FirstLBA;
				partitionOptions.ActualEnd   = partition.LastLBA + 1;
				if (options.Verbose)
					std::cout << "ImgGen INFO: partition '" << (i + 1) << "' (" << UTF::UTF16ToUTF8(std::u16string_view { (const char16_t*) partition.Name, 36 }) << ") declared with type '" << partition.Type << "', id '" << partition.ID << "', start '" << partition.FirstLBA << "', end '" << partition.LastLBA << "'\n";
			}
		}
		else
		{
			memset(&partitions, 0, 128 * sizeof(GPTPartition));
			for (size_t i = 0; i < options.PartitionCount; ++i)
			{
				auto& partition                = partitions[i];
				auto& partitionOptions         = options.PartitionOptions[i];
				partitionOptions.ForceReformat = true;

				partition.Type         = partitionOptions.Type.guid;
				partition.ID           = RandomGUID();
				partition.FirstLBA     = partitionOptions.ActualStart;
				partition.LastLBA      = partitionOptions.ActualEnd - 1;
				partition.Attribute    = 0;
				std::u16string u16Name = UTF::UTF8ToUTF16(partitionOptions.Name);
				for (size_t j = 0; j < std::min<size_t>(36, u16Name.size()); ++j)
					partition.Name[j] = (uint16_t) u16Name[j];

				if (options.Verbose)
					std::cout << "ImgGen INFO: partition '" << (i + 1) << "' (" << UTF::UTF16ToUTF8(std::u16string_view { (const char16_t*) partition.Name, 36 }) << ") declared with type '" << partition.Type << "', id '" << partition.ID << "', start '" << partition.FirstLBA << "', end '" << partition.LastLBA << "'\n";
			}
			header.PartitionCount = options.PartitionCount;
		}

		header.PartitionCRC32 = CRC32(partitions, header.PartitionCount * sizeof(GPTPartition));

		memcpy(&backupHeader, &header, sizeof(GPTHeader));

		std::swap(backupHeader.CurrentLBA, backupHeader.BackupLBA);
		backupHeader.PartitionsLBA = header.LastUsableLBA + 1;

		header.CRC32       = CRC32(&header, sizeof(GPTHeader));
		backupHeader.CRC32 = CRC32(&backupHeader, sizeof(GPTHeader));

		char zeros[512];
		memset(zeros, 0, 512);

		fstream.seekp(512);
		fstream.write((const char*) &header, sizeof(GPTHeader));
		fstream.write(zeros, 512 - sizeof(GPTHeader));
		fstream.seekp(header.PartitionsLBA * 512);
		fstream.write((const char*) partitions, 128 * sizeof(GPTPartition));
		fstream.seekp(header.BackupLBA * 512);
		fstream.write((const char*) &backupHeader, sizeof(GPTHeader));
		fstream.write(zeros, 512 - sizeof(GPTHeader));
		fstream.seekp(backupHeader.PartitionsLBA * 512);
		fstream.write((const char*) partitions, 128 * sizeof(GPTPartition));
	}

	uint64_t GetStartLBAOffset()
	{
		return 34;
	}

	uint64_t GetEndLBAOffset()
	{
		return 34;
	}

	uint64_t GetMinDriveSize()
	{
		return 0x8600;
	}

	uint64_t GetMaxDriveSize()
	{
		return ~0ULL;
	}

	uint8_t GetMaxPartitionCount()
	{
		return 128;
	}
} // namespace GPT