#pragma once

#include "State.hpp"
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>

namespace FAT
{
	static constexpr uint8_t ATTR_READ_ONLY = 0x01;
	static constexpr uint8_t ATTR_HIDDEN    = 0x02;
	static constexpr uint8_t ATTR_SYSTEM    = 0x04;
	static constexpr uint8_t ATTR_VOLUME_ID = 0x08;
	static constexpr uint8_t ATTR_LDIR      = ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID;
	static constexpr uint8_t ATTR_DIRECTORY = 0x10;
	static constexpr uint8_t ATTR_ARCHIVE   = 0x20;

	struct [[gnu::packed]] BPB
	{
		uint8_t  JmpBoot[3];
		uint8_t  OEMName[8];
		uint16_t BytsPerSec;
		uint8_t  SecPerClus;
		uint16_t RsvdSecCnt;
		uint8_t  NumFATs;
		uint16_t RootEntCnt;
		uint16_t TotSec16;
		uint8_t  Media;
		uint16_t FATSz16;
		uint16_t SecPerTrk;
		uint16_t NumHeads;
		uint32_t HiddSec;
		uint32_t TotSec32;
	};

	struct [[gnu::packed]] BPB32 : BPB
	{
		uint32_t FATSz32;
		uint16_t ExtFlags;
		uint16_t FSVer;
		uint32_t RootClus;
		uint16_t FSInfo;
		uint16_t BkBootSec;
		uint8_t  Reserved[12];
	};

	struct [[gnu::packed]] BS
	{
		uint8_t  DrvNum;
		uint8_t  Reserved1;
		uint8_t  BootSig;
		uint32_t VolID;
		uint8_t  VolLab[11];
		uint8_t  FilSysType[8];
	};

	struct [[gnu::packed]] EBPB16 : BPB, BS
	{
		uint8_t  BootCode[448];
		uint16_t Signature_word;
	};

	struct [[gnu::packed]] EBPB32 : BPB32, BS
	{
		uint8_t  BootCode[420];
		uint16_t Signature_word;
	};

	struct [[gnu::packed]] FSInfo
	{
		uint32_t LeadSig;
		uint8_t  Reserved1[480];
		uint32_t StrucSig;
		uint32_t Free_Count;
		uint32_t Nxt_Free;
		uint8_t  Reserved2[12];
		uint32_t TrailSig;
	};

	struct [[gnu::packed]] DIR
	{
		uint8_t  Name[11];
		uint8_t  Attr;
		uint8_t  NTRes;
		uint8_t  CrtTimeTenth;
		uint16_t CrtTime;
		uint16_t CrtDate;
		uint16_t LstAccDate;
		uint16_t FstClusHI;
		uint16_t WrtTime;
		uint16_t WrtDate;
		uint16_t FstClusLO;
		uint32_t FileSize;
	};

	struct [[gnu::packed]] LDIR
	{
		uint8_t  Ord;
		uint16_t Name1[5];
		uint8_t  Attr;
		uint8_t  Type;
		uint8_t  Chcksum;
		uint16_t Name2[6];
		uint16_t FstClusLO;
		uint16_t Name3[2];
	};

	uint16_t MakeDate(uint8_t day, uint8_t month, uint16_t year);
	uint16_t MakeTime(uint8_t hour, uint8_t minute, uint8_t second);
	void     GetDate(uint16_t date, uint8_t& day, uint8_t& month, uint16_t& year);
	void     GetTime(uint16_t time, uint8_t& hour, uint8_t& minute, uint8_t& second);
	uint16_t Today();
	uint16_t Now();

	enum class EFATType
	{
		FAT12,
		FAT16,
		FAT32
	};

	enum class EFileType
	{
		Invalid,
		Reserved,
		Directory,
		File
	};

	struct FATEntry
	{
		uint32_t Values[3];
	};

	struct DirectoryEntry
	{
		uint32_t FirstEntry;
		uint32_t Entry;
		uint32_t Cluster;
		uint32_t FileSize;

		std::u32string FullName;
		char           ShortName[11];

		uint16_t CreateDate;
		uint16_t CreateTime;
		uint16_t WriteDate;
		uint16_t WriteTime;
		uint16_t AccessDate;

		uint8_t Attribute;
	};

	struct FileState
	{
		struct State* State;
		uint32_t      FileCluster;
		uint32_t      Directory;
		uint32_t      Direntry;

		uint32_t ReadCluster;
		uint32_t WriteCluster;
		size_t   ReadOffset;
		size_t   WriteOffset;
	};

	struct DirectoryState
	{
		struct State*    State;
		uint32_t         Parent;
		uint32_t         FirstCluster;
		uint32_t         LastCluster;
		std::vector<DIR> Entries;

		std::vector<DirectoryEntry> ParsedEntries;

		static bool FillShortName(std::u32string_view fullName, char (&shortName)[11], bool& lowercaseFilename, bool& lowercaseExtension);
		void        GenShortNameTail(char (&shortName)[11]);

		DIR& NewRawEntry();

		void            AddLDIREntries(std::u32string_view fullName, uint8_t shortNameChecksum);
		DirectoryEntry* AddEntry(std::u32string_view fullName);
		DirectoryEntry* GetEntry(std::u32string_view fullName);
		DirectoryEntry* RenameEntry(std::u32string_view oldFullName, std::u32string_view newFullName);
		void            RemoveEntry(std::u32string_view fullName);
		void            NewDirectory(std::u32string_view fullName);
		void            NewFile(std::u32string_view fullName);
	};

	struct State
	{
		ImgGenOptions*    Options;
		PartitionOptions* Partition;
		std::fstream*     FStream;

		EFATType Type;

		uint8_t  ClusterSize;
		uint32_t FATSize;
		uint8_t  FATCount;
		uint32_t FirstFATSector;
		uint32_t FATSector;
		uint32_t FirstClusterSector;
		uint32_t MaxClusters;
		uint32_t FreeClusters;
		uint32_t NextFreeCluster;

		DirectoryState        RootDir;
		std::vector<FATEntry> FAT;
		uint8_t               CurFAT;
		bool                  FATMirrored;

		void     SetFAT(uint32_t cluster, uint32_t nextCluster);
		void     SetFATEnd(uint32_t cluster);
		bool     IsFATEnd(uint32_t cluster);
		uint32_t GetFAT(uint32_t cluster);
	};

	State LoadState(ImgGenOptions& options, PartitionOptions& partition, std::fstream& fstream);
	void  SaveState(State& state);

	uint64_t MaxFileSize(State& state);

	bool      Exists(State& state, std::string_view path);
	EFileType GetType(State& state, std::string_view path);

	FileState GetFile(State& state, std::string_view path);
	FileState CreateFile(State& state, std::string_view path);
	void      CloseFile(FileState& fileState);
	void      EnsureFileSize(FileState& fileState, uint64_t fileSize);
	uint64_t  GetFileSize(FileState& fileState);
	void      SetModifyTime(FileState& fileState, uint64_t time);
	uint64_t  GetModifyTime(FileState& fileState);
	uint64_t  Read(FileState& fileState, void* buffer, size_t count);
	uint64_t  Write(FileState& fileState, const void* buffer, size_t count);

	uint64_t GetMinPartitionSize();
	uint64_t GetMaxPartitionSize();

	std::string Normalize(std::string_view path);
} // namespace FAT