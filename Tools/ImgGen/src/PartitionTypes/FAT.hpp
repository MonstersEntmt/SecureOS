#pragma once

#include "State.hpp"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>

namespace stdext
{
	template <class C>
	struct string_hash
	{
		using hash_type      = std::hash<std::basic_string_view<C>>;
		using is_transparent = void;

		std::size_t operator()(const C* str) const { return hash_type {}(str); }
		std::size_t operator()(std::basic_string_view<C> str) const { return hash_type {}(str); }
		std::size_t operator()(const std::basic_string<C>& str) const { return hash_type {}(str); }
	};

	template <class C, class T>
	using unordered_string_map = std::unordered_map<std::basic_string<C>, T, string_hash<C>, std::equal_to<>>;
} // namespace stdext

namespace FAT
{
	using Clock     = std::chrono::system_clock;
	using TimePoint = Clock::time_point;

	static constexpr uint8_t ATTR_READ_ONLY   = 0x01;
	static constexpr uint8_t ATTR_HIDDEN      = 0x02;
	static constexpr uint8_t ATTR_SYSTEM      = 0x04;
	static constexpr uint8_t ATTR_VOLUME_ID   = 0x08;
	static constexpr uint8_t ATTR_LDIR        = ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID;
	static constexpr uint8_t ATTR_DIRECTORY   = 0x10;
	static constexpr uint8_t ATTR_ARCHIVE     = 0x20;
	static constexpr uint8_t LDIR_ORD_MASK    = 0x3F;
	static constexpr uint8_t LDIR_ORD_LAST    = 0x40;
	static constexpr uint8_t NTLN_NAME        = 0x08;
	static constexpr uint8_t NTLN_EXT         = 0x10;
	static constexpr uint8_t BPB32_NON_MIRROR = 0x80;
	static constexpr uint8_t BPB32_CUR_FAT    = 0x0F;

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
		uint16_t SignatureWord;
	};

	struct [[gnu::packed]] EBPB32 : BPB32, BS
	{
		uint8_t  BootCode[420];
		uint16_t SignatureWord;
	};

	struct [[gnu::packed]] FSInfo
	{
		uint32_t LeadSig;
		uint8_t  Reserved1[480];
		uint32_t StrucSig;
		uint32_t FreeCount;
		uint32_t NxtFree;
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

	uint16_t ToFATDate(uint8_t day, uint8_t month, uint16_t year);
	uint16_t ToFATTime(uint8_t hour, uint8_t minute, uint8_t second);
	void     FromFATDate(uint16_t date, uint8_t& day, uint8_t& month, uint16_t& year);
	void     FromFATTime(uint16_t time, uint8_t& hour, uint8_t& minute, uint8_t& second);
	void     FATNow(uint16_t& date, uint16_t& time, uint8_t& timeTenth);

	template <class Clock2, class Duration2>
	TimePoint ToTimePoint(std::chrono::time_point<Clock2, Duration2> otherTime)
	{
		TimePoint now       = Clock::now();
		auto      today     = std::chrono::floor<std::chrono::days>(now);
		auto      ymd       = std::chrono::year_month_day(today);
		auto      hms       = std::chrono::hh_mm_ss(now - today);
		uint32_t  date      = ToFATDate((unsigned) ymd.day(), (unsigned) ymd.month(), (int) ymd.year());
		uint32_t  time      = ToFATTime(hms.hours().count(), hms.minutes().count(), hms.seconds().count());
		uint8_t   timeTenth = (hms.seconds().count() & 1) * 100; // TODO(MarcasRealAccount): Do proper tenths

		uint8_t  day, month;
		uint16_t year;
		uint8_t  hour, minute, second;
		FromFATDate(date, day, month, year);
		FromFATTime(time, hour, minute, second);
		ymd = std::chrono::year_month_day(std::chrono::year { year }, std::chrono::month { month }, std::chrono::day { day });
		return std::chrono::sys_days { ymd } + std::chrono::hours { hour } + std::chrono::minutes { minute } + std::chrono::seconds { second } + std::chrono::milliseconds { timeTenth * 10 };
	}

	enum class EType
	{
		FAT12,
		FAT16,
		FAT32
	};

	struct FATEntry
	{
		uint32_t Values[3];
	};

	struct DirectoryEntry
	{
		uint32_t Cluster;
		uint32_t FileSize;

		std::u32string Filename;
		char           ShortName[11];

		uint8_t  CreateTimeTenth;
		uint16_t CreateTime;
		uint16_t CreateDate;
		uint16_t WriteTime;
		uint16_t WriteDate;
		uint16_t LastAccessDate;

		uint8_t Attribute;
	};

	struct DirectoryState
	{
	public:
		static bool    CompareFilenames(std::u32string_view lhs, std::u32string_view rhs);
		static bool    FillShortName(std::u32string_view filename, char (&shortName)[11], bool& lowercaseFilename, bool& lowercaseExtension);
		static void    FillFilename(std::u32string& filename, const char (&shortName)[11], uint8_t attribute, bool lowercaseFilename, bool lowercaseExtension);
		static uint8_t ShortNameChecksum(char (&shortName)[11]);

	public:
		struct State*               State;
		uint32_t                    Parent;
		uint32_t                    Cluster;
		bool                        Modified;
		std::vector<DIR>            RawEntries;
		std::vector<DirectoryEntry> ParsedEntries;
		uint32_t                    EstRawEntryCount;

		std::u32string Path;
		uint8_t        CreateTimeTenth;
		uint16_t       CreateTime;
		uint16_t       CreateDate;
		uint16_t       WriteTime;
		uint16_t       WriteDate;
		uint16_t       LastAccessDate;

		void GenShortNameTail(char (&shortName)[11]);
		void AddLDIREntries(std::u32string_view filename, uint8_t shortNameChecksum);

		void ParseRawEntries();
		void FlushRawEntries();
		void MarkModified();

		DirectoryEntry* AddEntry(std::u32string_view filename);
		DirectoryEntry* GetEntry(std::u32string_view filename);
		DirectoryEntry* RenameEntry(std::u32string_view oldFilename, std::u32string_view newFilename);
		void            RemoveEntry(std::u32string_view filename);

		DirectoryEntry* NewDirectory(std::u32string_view dirname);
		DirectoryEntry* NewFile(std::u32string_view filename);
		void            SetLabel(std::string_view label);
	};

	struct State
	{
		ImgGenOptions*    Options;
		PartitionOptions* Partition;
		std::fstream*     FStream;

		EType Type;

		uint8_t  ClusterSize;
		uint8_t  FATCount;
		uint8_t  CurFAT;
		uint32_t FATSize;
		uint32_t FirstFATSector;
		uint32_t CurFATSector;
		uint32_t FirstClusterSector;
		uint32_t MaxClusters;
		uint32_t FreeClusterCount;
		uint32_t NextFreeCluster;
		uint32_t MaxRootEntryCount;

		std::vector<FATEntry> FAT;

		DirectoryState                                         RootDir;
		stdext::unordered_string_map<char32_t, DirectoryState> CachedDirectories;

		void FlushRootDirectory(DirectoryState& directory);
		void FlushCache();

		DirectoryState* CacheDirectory(DirectoryState& parentDirectory, DirectoryEntry& entry);
		void            UncacheDirectory(DirectoryState& parentDirectory, DirectoryEntry& entry);
		DirectoryState* GetDirectory(std::u32string_view path);
		DirectoryState* LoadDirectory(std::u32string_view path, bool create = false);

		void     SetFAT(uint32_t cluster, uint32_t nextCluster);
		void     SetFATEnd(uint32_t cluster);
		uint32_t GetFAT(uint32_t cluster);
		bool     IsFATEnd(uint32_t cluster);

		uint32_t AllocCluster();
		uint32_t AllocClusters(uint32_t count);
		uint32_t EnsureClusters(uint32_t firstCluster, uint32_t count);
		void     FreeCluster(uint32_t cluster);
		void     FreeClusters(uint32_t firstCluster);
		uint32_t NextCluster(uint32_t cluster, bool alloc = false);
		uint32_t NthCluster(uint32_t firstCluster, uint32_t count, bool alloc = false);

		uint32_t WriteCluster(const void* data, uint32_t cluster, bool alloc = false);
		uint32_t ReadCluster(void* data, uint32_t cluster, bool alloc = false);
		uint32_t WriteClusters(const void* data, uint32_t count, uint32_t firstCluster, bool extend = false);
		uint32_t ReadClusters(void* data, uint32_t count, uint32_t firstCluster, bool extend = false);
		void     FillClusters(uint8_t value, uint32_t firstCluster);
	};

	enum class EFileType
	{
		Invalid,
		Reserved,
		Directory,
		File
	};

	struct FileState
	{
		State*          State;
		DirectoryState* Directory;
		DirectoryEntry* Entry;

		uint32_t ReadCluster;
		uint32_t WriteCluster;
		uint32_t ReadOffset;
		uint32_t WriteOffset;
	};

	EFileType FileTypeFromAttribute(uint8_t attribute);

	uint64_t MaxFileSize();
	uint64_t GetMinPartitionSize();
	uint64_t GetMaxPartitionSize();

	std::u32string NormalizePath(std::u32string_view path);

	State* LoadState(ImgGenOptions& options, PartitionOptions& partition, std::fstream& fstream);
	void   SaveState(State* state);

	bool      Exists(State* state, std::u32string_view path);
	EFileType GetType(State* state, std::u32string_view path);

	void      Delete(State* state, std::u32string_view path);
	void      CreateDirectory(State* state, std::u32string_view path);
	FileState GetFile(State* state, std::u32string_view path, bool create = false);
	void      CloseFile(FileState& fileState);
	void      EnsureFileSize(FileState& fileState, uint32_t fileSize);
	uint32_t  GetFileSize(FileState& fileState);
	void      SetWriteTime(FileState& fileState, TimePoint time);
	void      SetAccessTime(FileState& fileState, TimePoint time);
	TimePoint GetCreateTime(FileState& fileState);
	TimePoint GetWriteTime(FileState& fileState);
	TimePoint GetAccessTime(FileState& fileState);
	void      SeekRead(FileState& fileState, uint32_t amount, int direction = 0);
	void      SeekWrite(FileState& fileState, uint32_t amount, int direction = 0);
	uint32_t  Read(FileState& fileState, void* buffer, uint32_t size);
	uint32_t  Write(FileState& fileState, const void* buffer, uint32_t size);
} // namespace FAT