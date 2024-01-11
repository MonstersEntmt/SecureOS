#include "FAT32.h"
#include "LegacyBootProtection.h"
#include "UTF.h"

#include <cstring>
#include <cwctype>

#include <algorithm>
#include <iostream>
#include <string_view>

namespace FAT32
{
	static bool FillShortname(std::u16string_view filename, char (&shortName)[11], bool& lowercaseName, bool& lowercaseExt);
	static void GenShortnameTail(char (&shortName)[11], const std::vector<FAT32DirectoryEntry>& dirEntries);
	static void AppendLongFilanameEntries(std::vector<FAT32DirectoryEntry>& dirEntries, std::u16string_view filename, uint8_t checksum);
	static void WriteBPB(FSState& state, std::fstream& imageStream, uint32_t sector);
	static void WriteFSInfo(FSState& state, std::fstream& imageStream, uint32_t sector);
	static void WriteFAT(FSState& state, std::fstream& imageStream, uint32_t sector, const uint32_t* fat, size_t count);
	static void WriteCluster(FSState& state, std::fstream& imageStream, uint32_t cluster, const void* data, uint32_t dataLength);
	static void WriteClusters(FSState& state, std::fstream& imageStream, uint32_t firstCluster, const void* data, uint32_t dataLength, bool alloc = false);
	static bool CompareFilenames(std::u16string_view filename1, std::string_view filename2, bool caseSensitive = true);

	void InitState(FSState& state, std::string_view volumeLabel)
	{
		memcpy(state.Header.JmpBoot, "\xEB\x58\x90", 3);
		// memcpy(state.Header.OEMName, "SECUREOS", 8);
		memcpy(state.Header.OEMName, "MSWIN4.1", 8);
		state.Header.BytesPerSector      = 512;
		state.Header.SectorsPerCluster   = 8;
		state.Header.ReservedSectorCount = 32;
		state.Header.NumFATs             = 2;
		state.Header.RootEntryCount      = 0;
		state.Header.TotalSectors16      = 0;
		state.Header.Media               = 0xF8;
		state.Header.FATSize16           = 0;
		state.Header.SectorsPerTrack     = 0x3F;
		state.Header.NumHeads            = 0x80;
		state.Header.HiddenSectors       = (uint32_t) std::min<uint64_t>(state.FirstLBA, 0xFFFF'FFFF);
		state.Header.TotalSectors32      = (uint32_t) std::min<uint64_t>(state.LastLBA - state.FirstLBA, 0xFFFF'FFFF);

		uint32_t temp                 = (256 * state.Header.SectorsPerCluster + state.Header.NumFATs) / 2;
		state.Header.FATSize32        = (state.Header.TotalSectors32 - state.Header.ReservedSectorCount + temp - 1) / temp;
		state.Header.ExtendedFlags    = 0x0000;
		state.Header.FSVersion        = 0;
		state.Header.FSInfoSector     = 1;
		state.Header.BackupBootSector = 6;
		memset(state.Header.Reserved, 0, sizeof(state.Header.Reserved));
		state.Header.DriveNumber           = 0x80;
		state.Header.Reserved1             = 0;
		state.Header.ExtendedBootSignature = 0x29;
		state.Header.VolumeID              = 0;
		memset(state.Header.VolumeLabel, 0, 11);
		memcpy(state.Header.VolumeLabel, volumeLabel.data(), std::min<size_t>(11, volumeLabel.size()));
		memcpy(state.Header.FileSystemType, "FAT32   ", 8);

		state.MaxClusters = (state.Header.TotalSectors32 - state.Header.ReservedSectorCount - state.Header.FATSize32 * state.Header.NumFATs + 2) / state.Header.SectorsPerCluster;
		state.FAT.resize(state.Header.FATSize32 * 128);
		state.FAT[0]             = 0xFFFF'FFF8;
		state.FAT[1]             = 0xFFFF'FFFF;
		state.FreeClusters       = state.MaxClusters;
		state.FirstFreeCluster   = 2;
		state.Header.RootCluster = AllocCluster(state);

		auto& rootDirectory           = state.Directories.emplace_back();
		rootDirectory.DirectoryName   = "/";
		rootDirectory.FirstCluster    = state.Header.RootCluster;
		rootDirectory.ParentDirectory = 0;

		auto& volumeIDEntry = rootDirectory.Entries.emplace_back();
		memcpy(volumeIDEntry.Name, state.Header.VolumeLabel, 11);
		volumeIDEntry.Attributes        = 0x08;
		volumeIDEntry.NTRes             = 0;
		volumeIDEntry.CreationTimeTenth = 0;
		volumeIDEntry.CreationTime      = 0;
		volumeIDEntry.CreationDate      = 0;
		volumeIDEntry.LastAccessDate    = 0;
		volumeIDEntry.FirstClusterHigh  = 0;
		volumeIDEntry.WriteTime         = 0;
		volumeIDEntry.WriteDate         = 0;
		volumeIDEntry.FirstClusterLow   = 0;
		volumeIDEntry.FileSize          = 0;
	}

	uint32_t AllocCluster(FSState& state)
	{
		uint32_t curCluster = state.FirstFreeCluster;
		do
		{
			uint32_t value = state.FAT[curCluster];
			if (value == 0U)
			{
				--state.FreeClusters;
				++state.FirstFreeCluster;
				state.FAT[curCluster] = 0xFFFF'FFFF;
				if (state.Verbose)
					std::cout << "Cluster '" << curCluster << "' allocated\n";
				return curCluster;
			}
			curCluster = (curCluster + 1) % state.MaxClusters;
		}
		while (curCluster != state.FirstFreeCluster);
		return 0;
	}

	uint32_t AllocClusters(FSState& state, uint32_t dataLength)
	{
		uint32_t firstCluster = AllocCluster(state);
		uint32_t curCluster   = firstCluster;
		for (size_t i = 1; i < (dataLength + state.Header.SectorsPerCluster * 512 - 1) / (state.Header.SectorsPerCluster * 512); ++i)
		{
			uint32_t nextCluster = AllocCluster(state);
			if (nextCluster == 0U)
			{
				FreeClusters(state, firstCluster);
				return 0U;
			}
			state.FAT[curCluster] = nextCluster;
			curCluster            = nextCluster;
		}
		return firstCluster;
	}

	void FreeCluster(FSState& state, uint32_t cluster)
	{
		state.FAT[cluster] = 0x0U;
		++state.FreeClusters;
		if (cluster < state.FirstFreeCluster)
			state.FirstFreeCluster = cluster;
		if (state.Verbose)
			std::cout << "Cluster '" << cluster << "' freed\n";
	}

	void FreeClusters(FSState& state, uint32_t firstCluster)
	{
		while (firstCluster != 0xFFFF'FFFF)
		{
			uint32_t nextCluster = state.FAT[firstCluster];
			FreeCluster(state, firstCluster);
			firstCluster = nextCluster;
		}
	}

	uint32_t NextCluster(FSState& state, uint32_t cluster, bool alloc)
	{
		if (cluster == 0xFFFF'FFFF)
			return 0xFFFF'FFFF;
		uint32_t nextCluster = state.FAT[cluster];
		if (alloc && nextCluster == 0xFFFF'FFFF)
		{
			nextCluster        = AllocCluster(state);
			state.FAT[cluster] = nextCluster;
		}
		return nextCluster;
	}

	FileState GetDirDirectory(FSState& state, uint32_t parent, std::string_view name)
	{
		if (parent == 0)
			return { &state, state.Header.RootCluster, 0, 0 };

		auto itr = std::find_if(state.Directories.begin(), state.Directories.end(), [parent](const DirectoryState& dir) -> bool { return dir.FirstCluster == parent; });
		if (itr == state.Directories.end())
			return { &state, ~0U, 0, 0 };

		char16_t filenameBuf[261];
		for (size_t i = 0; i < itr->Entries.size();)
		{
			auto& entry = itr->Entries[i];
			if (!(entry.Attributes & 0x30))
			{
				++i;
				continue;
			}

			if ((entry.Attributes & 0xF) == 0xF)
			{
				FAT32LongFilenameDirectoryEntry* longEntry = (FAT32LongFilenameDirectoryEntry*) &entry;

				memcpy(filenameBuf + 260 - 13, longEntry->Name1, 10);
				memcpy(filenameBuf + 260 - 8, longEntry->Name2, 12);
				memcpy(filenameBuf + 260 - 2, longEntry->Name3, 4);
				uint8_t index = 1;
				do
				{
					++longEntry;
					++i;
					memcpy(filenameBuf + 260 - 13 * index, longEntry->Name1, 10);
					memcpy(filenameBuf + 260 - 13 * index + 5, longEntry->Name2, 12);
					memcpy(filenameBuf + 260 - 13 * index + 11, longEntry->Name3, 4);
				}
				while (!(longEntry->Ordinal & 0x40));
				memmove(filenameBuf, filenameBuf + 260 - 13 * index, (13 * index) * 2);
				filenameBuf[13 * index] = u'\0';
				++i;

				if (!CompareFilenames(filenameBuf, name, true))
				{
					++i;
					continue;
				}
			}
			else
			{
				size_t j = 0;
				for (j = 0; j < 8; ++j)
				{
					if (entry.Name[j] == ' ')
						break;
					filenameBuf[j] = entry.Name[j];
				}
				for (size_t k = 0; k < 3; ++k)
				{
					if (entry.Name[8 + k] == ' ')
						break;
					filenameBuf[j++] = entry.Name[8 + k];
				}
				filenameBuf[j] = u'\0';

				if (!CompareFilenames(filenameBuf, name, false))
				{
					++i;
					continue;
				}
			}

			return { &state, (uint32_t) (itr->Entries[i].FirstClusterLow | (itr->Entries[i].FirstClusterHigh << 16)), parent, (uint32_t) i };
		}
		return { &state, ~0U, 0, 0 };
	}

	FileState EnsureDirDirectory(FSState& state, uint32_t parent, std::string_view name)
	{
		if (parent == 0)
			return { &state, state.Header.RootCluster, 0, 0 };

		auto itr = std::find_if(state.Directories.begin(), state.Directories.end(), [parent](const DirectoryState& dir) -> bool { return dir.FirstCluster == parent; });
		if (itr == state.Directories.end())
		{
			if (state.Verbose)
				std::cout << "Failed to find parent directory '" << parent << "'\n";
			return { &state, ~0U, 0, 0 };
		}

		auto dir = GetDirDirectory(state, parent, name);
		if (dir.FileCluster != ~0U)
			return dir;

		if (state.Verbose)
			std::cout << "Directory '" << name << "' in parent cluster '" << parent << "' does not exist, attempting to create it\n";

		auto nameutf16 = UTF8ToUTF16(name);
		char shortName[11];
		bool lowercaseName = false, lowercaseExt = false;
		bool isShortName = FillShortname(nameutf16, shortName, lowercaseName, lowercaseExt);
		if (!isShortName)
		{
			GenShortnameTail(shortName, itr->Entries);

			uint8_t checksum = 0;
			for (size_t i = 0; i < 11; ++i)
				checksum = (checksum & 1 ? 0x80 : 0) + (checksum >> 1) + shortName[i];
			AppendLongFilanameEntries(itr->Entries, nameutf16, checksum);
		}

		uint32_t firstCluster = AllocCluster(state);

		auto& dirEntry = itr->Entries.emplace_back();
		memcpy(dirEntry.Name, shortName, 11);
		dirEntry.Attributes        = 0x10;
		dirEntry.NTRes             = lowercaseName << 3 | lowercaseExt << 4;
		dirEntry.CreationTimeTenth = 0;
		dirEntry.CreationTime      = 0;
		dirEntry.CreationDate      = 0;
		dirEntry.LastAccessDate    = 0;
		dirEntry.FirstClusterHigh  = firstCluster >> 16;
		dirEntry.WriteTime         = 0;
		dirEntry.WriteDate         = 0;
		dirEntry.FirstClusterLow   = firstCluster & 0xFFFF;
		dirEntry.FileSize          = 0;

		auto& subDir           = state.Directories.emplace_back();
		subDir.DirectoryName   = name;
		subDir.ParentDirectory = parent;
		subDir.FirstCluster    = firstCluster;

		auto& dotDir = subDir.Entries.emplace_back();
		memcpy(dotDir.Name, ".          ", 11);
		dotDir.Attributes        = 0x10;
		dotDir.NTRes             = 0;
		dotDir.CreationTimeTenth = dirEntry.CreationTimeTenth;
		dotDir.CreationTime      = dirEntry.CreationTime;
		dotDir.CreationDate      = dirEntry.CreationDate;
		dotDir.LastAccessDate    = dirEntry.LastAccessDate;
		dotDir.FirstClusterHigh  = dirEntry.FirstClusterHigh;
		dotDir.WriteTime         = dirEntry.WriteTime;
		dotDir.WriteDate         = dirEntry.WriteDate;
		dotDir.FirstClusterLow   = dirEntry.FirstClusterLow;
		dotDir.FileSize          = 0;

		auto& dotdotDir = subDir.Entries.emplace_back();
		memcpy(dotdotDir.Name, "..         ", 11);
		dotdotDir.Attributes        = 0x10;
		dotdotDir.NTRes             = 0;
		dotdotDir.CreationTimeTenth = dirEntry.CreationTimeTenth;
		dotdotDir.CreationTime      = dirEntry.CreationTime;
		dotdotDir.CreationDate      = dirEntry.CreationDate;
		dotdotDir.LastAccessDate    = dirEntry.LastAccessDate;
		dotdotDir.FirstClusterHigh  = parent == state.Header.RootCluster ? 0 : parent >> 16;
		dotdotDir.WriteTime         = dirEntry.WriteTime;
		dotdotDir.WriteDate         = dirEntry.WriteDate;
		dotdotDir.FirstClusterLow   = parent == state.Header.RootCluster ? 0 : parent & 0xFFFF;
		dotdotDir.FileSize          = 0;

		return { &state, firstCluster, parent, (uint32_t) (itr->Entries.size() - 1) };
	}

	FileState GetDirFile(FSState& state, uint32_t directory, std::string_view name)
	{
		auto itr = std::find_if(state.Directories.begin(), state.Directories.end(), [directory](const DirectoryState& dir) -> bool { return dir.FirstCluster == directory; });
		if (itr == state.Directories.end())
			return { &state, ~0U, 0, 0 };

		char16_t filenameBuf[261];
		for (size_t i = 0; i < itr->Entries.size();)
		{
			auto& entry = itr->Entries[i];
			if (entry.Attributes & 0x30)
			{
				++i;
				continue;
			}

			if ((entry.Attributes & 0xF) == 0xF)
			{
				FAT32LongFilenameDirectoryEntry* longEntry = (FAT32LongFilenameDirectoryEntry*) &entry;

				memcpy(filenameBuf + 260 - 13, longEntry->Name1, 10);
				memcpy(filenameBuf + 260 - 8, longEntry->Name2, 12);
				memcpy(filenameBuf + 260 - 2, longEntry->Name3, 4);
				uint8_t index = 1;
				do
				{
					++longEntry;
					++i;
					memcpy(filenameBuf + 260 - 13 * index, longEntry->Name1, 10);
					memcpy(filenameBuf + 260 - 13 * index + 5, longEntry->Name2, 12);
					memcpy(filenameBuf + 260 - 13 * index + 11, longEntry->Name3, 4);
				}
				while (!(longEntry->Ordinal & 0x40));
				memmove(filenameBuf, filenameBuf + 260 - 13 * index, (13 * index) * 2);
				filenameBuf[13 * index] = u'\0';
				++i;

				if (!CompareFilenames(filenameBuf, name, true))
				{
					++i;
					continue;
				}
			}
			else
			{
				size_t j = 0;
				for (j = 0; j < 8; ++j)
				{
					if (entry.Name[j] == ' ')
						break;
					filenameBuf[j] = entry.Name[j];
				}
				for (size_t k = 0; k < 3; ++k)
				{
					if (entry.Name[8 + k] == ' ')
						break;
					filenameBuf[j++] = entry.Name[8 + k];
				}
				filenameBuf[j] = u'\0';

				if (!CompareFilenames(filenameBuf, name, false))
				{
					++i;
					continue;
				}
			}

			return { &state, (uint32_t) (itr->Entries[i].FirstClusterLow | (itr->Entries[i].FirstClusterHigh << 16)), directory, (uint32_t) i };
		}
		return { &state, ~0U, 0, 0 };
	}

	FileState EnsureDirFile(FSState& state, uint32_t directory, std::string_view name)
	{
		auto itr = std::find_if(state.Directories.begin(), state.Directories.end(), [directory](const DirectoryState& dir) -> bool { return dir.FirstCluster == directory; });
		if (itr == state.Directories.end())
		{
			if (state.Verbose)
				std::cout << "Failed to find directory '" << directory << "'\n";
			return { &state, ~0U, 0, 0 };
		}

		auto file = GetDirFile(state, directory, name);
		if (file.FileCluster != ~0U)
			return file;

		if (state.Verbose)
			std::cout << "File '" << name << "' is not part of directory '" << directory << "', attempting to create it\n";

		auto nameutf16 = UTF8ToUTF16(name);
		char shortName[11];
		bool lowercaseName = false, lowercaseExt = false;
		bool isShortName = FillShortname(nameutf16, shortName, lowercaseName, lowercaseExt);
		if (!isShortName)
		{
			GenShortnameTail(shortName, itr->Entries);

			uint8_t checksum = 0;
			for (size_t i = 0; i < 11; ++i)
				checksum = (checksum & 1 ? 0x80 : 0) + (checksum >> 1) + shortName[i];
			AppendLongFilanameEntries(itr->Entries, nameutf16, checksum);
		}

		uint32_t firstCluster = AllocCluster(state);

		auto& dirEntry = itr->Entries.emplace_back();
		memcpy(dirEntry.Name, shortName, 11);
		dirEntry.Attributes        = 0x00;
		dirEntry.NTRes             = lowercaseName << 3 | lowercaseExt << 4;
		dirEntry.CreationTimeTenth = 0;
		dirEntry.CreationTime      = 0;
		dirEntry.CreationDate      = 0;
		dirEntry.LastAccessDate    = 0;
		dirEntry.FirstClusterHigh  = firstCluster >> 16;
		dirEntry.WriteTime         = 0;
		dirEntry.WriteDate         = 0;
		dirEntry.FirstClusterLow   = firstCluster & 0xFFFF;
		dirEntry.FileSize          = 0;

		return { &state, firstCluster, directory, (uint32_t) (itr->Entries.size() - 1) };
	}

	FileState GetDirectory(FSState& state, std::string_view path)
	{
		if (path.empty())
			return GetDirDirectory(state, 0, "");

		uint32_t parent = 0;
		size_t   offset = 0;
		while (offset < path.size())
		{
			size_t end = path.find_first_of('/', offset);
			auto   dir = GetDirDirectory(state, parent, path.substr(offset, end - offset));
			if (end >= path.size())
				return dir;
			parent = dir.FileCluster;
			offset = end + 1;
		}
		return { &state, ~0U, 0, 0 };
	}

	FileState EnsureDirectory(FSState& state, std::string_view path)
	{
		auto dir = GetDirectory(state, path);
		if (dir.FileCluster != ~0U)
			return dir;

		if (state.Verbose)
			std::cout << "Directory '" << path << "' does not exist, ensuring all directory parts exists\n";

		uint32_t parent = 0;
		size_t   offset = 0;
		while (offset < path.size())
		{
			size_t end = path.find_first_of('/', offset);
			auto   dir = EnsureDirDirectory(state, parent, path.substr(offset, end - offset));
			if (end >= path.size() - 1)
				return dir;
			parent = dir.FileCluster;
			offset = end + 1;
		}
		return { &state, ~0U, 0, 0 };
	}

	FileState GetFile(FSState& state, std::string_view path)
	{
		size_t directoryEnd = path.find_last_of('/');
		auto   dir          = GetDirectory(state, path.substr(0, directoryEnd));
		if (dir.FileCluster == ~0U)
			return { &state, ~0U, 0, 0 };
		return GetDirFile(state, dir.FileCluster, path.substr(directoryEnd + 1));
	}

	FileState EnsureFile(FSState& state, std::string_view path)
	{
		size_t directoryEnd = path.find_last_of('/');
		auto   dir          = EnsureDirectory(state, path.substr(0, directoryEnd));
		return EnsureDirFile(state, dir.FileCluster, path.substr(directoryEnd + 1));
	}

	void WriteState(FSState& state, std::fstream& imageStream)
	{
		WriteBPB(state, imageStream, 0);
		WriteBPB(state, imageStream, state.Header.BackupBootSector);
		WriteFSInfo(state, imageStream, 1);
		WriteFSInfo(state, imageStream, state.Header.BackupBootSector + 1);
		WriteFAT(state, imageStream, state.Header.ReservedSectorCount, state.FAT.data(), state.FAT.size());
		WriteFAT(state, imageStream, state.Header.FATSize32 + state.Header.ReservedSectorCount, state.FAT.data(), state.FAT.size());

		for (auto& dir : state.Directories)
			WriteClusters(state, imageStream, dir.FirstCluster, dir.Entries.data(), dir.Entries.size() * sizeof(FAT32DirectoryEntry), true);
	}

	void WriteFile(FSState& state, std::fstream& imageStream, std::string_view filepath, const void* data, uint32_t dataLength)
	{
		auto filestate = EnsureFile(state, filepath);
		if (state.Verbose)
			std::cout << "WriteFile got cluster " << filestate.FileCluster << ", in directory cluster " << filestate.Directory << " entry " << filestate.Direntry << '\n';
		WriteClusters(state, imageStream, filestate.FileCluster, data, dataLength, true);
		auto itr = std::find_if(state.Directories.begin(), state.Directories.end(), [&filestate](const DirectoryState& dir) -> bool { return dir.FirstCluster == filestate.Directory; });
		if (itr == state.Directories.end())
			return;
		itr->Entries[filestate.Direntry].FileSize = dataLength;
	}

	bool FillShortname(std::u16string_view filename, char (&shortName)[11], bool& lowercaseName, bool& lowercaseExt)
	{
		bool   needsLongName      = false;
		size_t lowercaseNameCount = 0;
		size_t lowercaseExtCount  = 0;

		size_t extBegin = std::min<size_t>(filename.size(), filename.find_last_of(u'.'));
		if (extBegin > 8 || (filename.size() - extBegin) > 4)
			needsLongName = true;

		memset(shortName, ' ', 11);
		for (size_t i = 0; i < std::min<size_t>(8, extBegin); ++i)
		{
			char16_t c = filename[i];
			if (c >= 0x80)
			{
				needsLongName = true;
				shortName[i]  = '_';
			}
			else
			{
				shortName[i] = (char) std::towupper(c);
			}
			if (c >= 'a' && c <= 'z')
				++lowercaseNameCount;
		}
		for (size_t i = extBegin + 1, j = 8; i < filename.size(); ++i, ++j)
		{
			char16_t c = filename[i];
			if (c >= 0x80)
			{
				needsLongName = true;
				shortName[j]  = '_';
			}
			else
			{
				shortName[j] = (char) std::towupper(c);
			}
			if (c >= 'a' && c <= 'z')
				++lowercaseExtCount;
		}

		if (lowercaseNameCount == 0)
			lowercaseName = false;
		else if (lowercaseNameCount == extBegin)
			lowercaseName = true;
		else
			needsLongName = true;
		if (lowercaseExtCount == 0)
			lowercaseExt = false;
		else if (lowercaseExtCount == filename.size() - extBegin - 1)
			lowercaseExt = true;
		else
			needsLongName = true;
		return !needsLongName;
	}

	void GenShortnameTail(char (&shortName)[11], const std::vector<FAT32DirectoryEntry>& dirEntries)
	{
		uint32_t nextN = 1;
		for (auto& entry : dirEntries)
		{
			if ((entry.Attributes & 0xF) == 0xF)
				continue;

			if (memcmp(entry.Name + 8, shortName + 8, 3) != 0) // Different extensions
				continue;

			bool   foundMatch = true;
			size_t i          = 0;
			for (; i < 8; ++i)
			{
				char c = entry.Name[i];
				if (c == '~')
					break;
				if (shortName[i] != c)
				{
					foundMatch = false;
					break;
				}
			}
			if (!foundMatch || i == 8)
				continue;

			uint64_t v = std::strtoull((const char*) entry.Name + i, nullptr, 10);
			if (v > nextN)
				nextN = (uint32_t) std::max<uint64_t>(v, 0xFFFF'FFFE) + 1;
		}

		if (nextN < 10)
		{
			shortName[6] = '~';
			shortName[7] = '0' + nextN;
		}
		else if (nextN < 100)
		{
			shortName[5] = '~';
			shortName[7] = '0' + nextN % 10;
			shortName[6] = '0' + nextN / 10;
		}
		else if (nextN < 1'000)
		{
			shortName[4] = '~';
			shortName[7] = '0' + nextN % 10;
			nextN       /= 10;
			shortName[6] = '0' + nextN % 10;
			shortName[5] = '0' + nextN / 10;
		}
		else if (nextN < 10'000)
		{
			shortName[3] = '~';
			shortName[7] = '0' + nextN % 10;
			nextN       /= 10;
			shortName[6] = '0' + nextN % 10;
			nextN       /= 10;
			shortName[5] = '0' + nextN % 10;
			shortName[4] = '0' + nextN / 10;
		}
		else if (nextN < 100'000)
		{
			shortName[2] = '~';
			shortName[7] = '0' + nextN % 10;
			nextN       /= 10;
			shortName[6] = '0' + nextN % 10;
			nextN       /= 10;
			shortName[5] = '0' + nextN % 10;
			nextN       /= 10;
			shortName[4] = '0' + nextN % 10;
			shortName[3] = '0' + nextN / 10;
		}
		else if (nextN < 1'000'000)
		{
			shortName[1] = '~';
			shortName[7] = '0' + nextN % 10;
			nextN       /= 10;
			shortName[6] = '0' + nextN % 10;
			nextN       /= 10;
			shortName[5] = '0' + nextN % 10;
			nextN       /= 10;
			shortName[4] = '0' + nextN % 10;
			nextN       /= 10;
			shortName[3] = '0' + nextN % 10;
			shortName[2] = '0' + nextN / 10;
		}
	}

	void AppendLongFilanameEntries(std::vector<FAT32DirectoryEntry>& dirEntries, std::u16string_view filename, uint8_t checksum)
	{
		size_t entryCount = (filename.size() + 12) / 13;
		for (size_t i = entryCount, j = 0; i-- > 0; ++j)
		{
			FAT32LongFilenameDirectoryEntry* entry = (FAT32LongFilenameDirectoryEntry*) &dirEntries.emplace_back();
			entry->Ordinal                         = (uint8_t) (i + 1);
			if (j == 0)
				entry->Ordinal |= 0x40;
			entry->Attributes      = 0x0F;
			entry->Type            = 0;
			entry->Checksum        = checksum;
			entry->FirstClusterLow = 0;

			memset(entry->Name1, 0xFF, 10);
			memset(entry->Name2, 0xFF, 12);
			memset(entry->Name3, 0xFF, 4);
			size_t charsLeft = std::min<size_t>(13, filename.size() - i * 13);
			for (size_t k = 0; k < charsLeft; ++k)
			{
				if (k < 5)
					entry->Name1[k] = filename[i * 13 + k];
				else if (k < 11)
					entry->Name2[k - 5] = filename[i * 13 + k];
				else
					entry->Name3[k - 11] = filename[i * 13 + k];
			}
			if (charsLeft < 13)
			{
				if (charsLeft < 5)
					entry->Name1[charsLeft] = u'\0';
				else if (charsLeft < 11)
					entry->Name2[charsLeft - 5] = u'\0';
				else
					entry->Name3[charsLeft - 11] = u'\0';
			}
		}
	}

	void WriteBPB(FSState& state, std::fstream& imageStream, uint32_t sector)
	{
		imageStream.seekp((state.FirstLBA + sector) * 512);
		imageStream.write((const char*) &state.Header, sizeof(FAT32Header));
		imageStream.write((const char*) c_LegacyBootProtection, 420);
		imageStream.write("\x55\xAA", 2);
	}

	void WriteFSInfo(FSState& state, std::fstream& imageStream, uint32_t sector)
	{
		char buf[512];
		memset(buf, 0, 512);

		FAT32FSInfo fsInfo;
		fsInfo.StructureSignature = 0x61417272;
		fsInfo.FreeCount          = state.FreeClusters;
		fsInfo.NextFree           = state.FirstFreeCluster;
		memset(fsInfo.Reserved2, 0, sizeof(fsInfo.Reserved2));
		fsInfo.TrailSignature = 0xAA550000;

		imageStream.seekp((state.FirstLBA + sector) * 512);
		imageStream.write("\x52\x52\x61\x41", 4);
		imageStream.write(buf, 480);
		imageStream.write((const char*) &fsInfo, sizeof(fsInfo));
	}

	void WriteFAT(FSState& state, std::fstream& imageStream, uint32_t sector, const uint32_t* fat, size_t count)
	{
		char buf[512];
		memset(buf, 0, 512);

		imageStream.seekp((state.FirstLBA + sector) * 512);
		imageStream.write((const char*) fat, count * 4);
		if (count % 128 > 0)
			imageStream.write(buf, (128 - count % 128) * 4);
	}

	void WriteCluster(FSState& state, std::fstream& imageStream, uint32_t cluster, const void* data, uint32_t dataLength)
	{
		imageStream.seekp((state.FirstLBA + state.Header.ReservedSectorCount + state.Header.NumFATs * state.Header.FATSize32 + (cluster - 2) * state.Header.SectorsPerCluster) * 512);
		imageStream.write((const char*) data, std::min<size_t>(dataLength, state.Header.SectorsPerCluster * 512));
	}

	void WriteClusters(FSState& state, std::fstream& imageStream, uint32_t firstCluster, const void* data, uint32_t dataLength, bool alloc)
	{
		const uint8_t* pD       = (const uint8_t*) data;
		size_t         dataLeft = dataLength;

		while (dataLeft)
		{
			size_t toWrite = std::min<size_t>(state.Header.SectorsPerCluster * 512, dataLeft);
			WriteCluster(state, imageStream, firstCluster, pD, toWrite);
			pD       += toWrite;
			dataLeft -= toWrite;
			if (dataLeft > 0)
			{
				uint32_t nextCluster = NextCluster(state, firstCluster, alloc);
				if (nextCluster == 0xFFFF'FFFF)
					break;
				firstCluster = nextCluster;
			}
		}
	}

	bool CompareFilenames(std::u16string_view filename1, std::string_view filename2, bool caseSensitive)
	{
		auto filename2utf16 = UTF8ToUTF16(filename2);
		if (filename1.size() != filename2utf16.size())
			return false;

		for (size_t i = 0; i < filename1.size(); ++i)
		{
			char16_t a = caseSensitive ? filename1[i] : (char16_t) std::towupper(filename1[i]);
			char16_t b = caseSensitive ? filename2utf16[i] : (char16_t) std::towupper(filename2utf16[i]);
			if (a != b)
				return false;
		}
		return true;
	}
} // namespace FAT32