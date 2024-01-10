#pragma once

#include <cstdint>

#include <fstream>
#include <vector>

namespace FAT32
{
	struct FAT32LongFilenameDirectoryEntry
	{
		uint8_t  Ordinal;
		uint16_t Name1[5];
		uint8_t  Attributes;
		uint8_t  Type;
		uint8_t  Checksum;
		uint16_t Name2[6];
		uint16_t FirstClusterLow;
		uint16_t Name3[2];
	} __attribute__((packed));

	struct FAT32DirectoryEntry
	{
		uint8_t  Name[11];
		uint8_t  Attributes;
		uint8_t  NTRes;
		uint8_t  CreationTimeTenth;
		uint16_t CreationTime;
		uint16_t CreationDate;
		uint16_t LastAccessDate;
		uint16_t FirstClusterHigh;
		uint16_t WriteTime;
		uint16_t WriteDate;
		uint16_t FirstClusterLow;
		uint32_t FileSize;
	};

	struct FAT32FSInfo
	{
		uint32_t StructureSignature;
		uint32_t FreeCount;
		uint32_t NextFree;
		uint8_t  Reserved2[12];
		uint32_t TrailSignature;
	} __attribute__((packed));

	struct FAT32Header
	{
		uint8_t  JmpBoot[3];
		uint8_t  OEMName[8];
		uint16_t BytesPerSector;
		uint8_t  SectorsPerCluster;
		uint16_t ReservedSectorCount;
		uint8_t  NumFATs;
		uint16_t RootEntryCount;
		uint16_t TotalSectors16;
		uint8_t  Media;
		uint16_t FATSize16;
		uint16_t SectorsPerTrack;
		uint16_t NumHeads;
		uint32_t HiddenSectors;
		uint32_t TotalSectors32;

		uint32_t FATSize32;
		uint16_t ExtendedFlags;
		uint16_t FSVersion;
		uint32_t RootCluster;
		uint16_t FSInfoSector;
		uint16_t BackupBootSector;
		uint8_t  Reserved[12];
		uint8_t  DriveNumber;
		uint8_t  Reserved1;
		uint8_t  ExtendedBootSignature;
		uint32_t VolumeID;
		uint8_t  VolumeLabel[11];
		uint8_t  FileSystemType[8];
	} __attribute__((packed));

	struct DirectoryState
	{
		std::string DirectoryName;

		uint32_t ParentDirectory;
		uint32_t FirstCluster;

		std::vector<FAT32DirectoryEntry> Entries;
	};

	struct FSState
	{
		FAT32Header Header alignas(16);
		uint64_t    FirstLBA;
		uint64_t    LastLBA;

		std::vector<uint32_t> FAT;
		uint32_t              MaxClusters;
		uint32_t              FreeClusters;
		uint32_t              FirstFreeCluster;

		std::vector<DirectoryState> Directories;

		bool Verbose;
		bool NoShortName;
	};

	struct FileState
	{
		FSState* FS;
		uint32_t FileCluster;
		uint32_t Directory;
		uint32_t Direntry;
	};

	void     InitState(FSState& state, std::string_view volumeLabel);
	uint32_t AllocCluster(FSState& state);
	uint32_t AllocClusters(FSState& state, uint32_t dataLength);
	void     FreeCluster(FSState& state, uint32_t cluster);
	void     FreeClusters(FSState& state, uint32_t firstCluster);
	uint32_t NextCluster(FSState& state, uint32_t cluster, bool alloc = false);

	FileState GetDirDirectory(FSState& state, uint32_t parent, std::string_view name);
	FileState EnsureDirDirectory(FSState& state, uint32_t parent, std::string_view name);
	FileState GetDirFile(FSState& state, uint32_t directory, std::string_view name);
	FileState EnsureDirFile(FSState& state, uint32_t directory, std::string_view name);

	FileState GetDirectory(FSState& state, std::string_view path);
	FileState EnsureDirectory(FSState& state, std::string_view path);
	FileState GetFile(FSState& state, std::string_view path);
	FileState EnsureFile(FSState& state, std::string_view path);

	void WriteState(FSState& state, std::fstream& imageStream);
	void WriteFile(FSState& state, std::fstream& imageStream, std::string_view filepath, const void* data, uint32_t dataLength);
} // namespace FAT32