#include "FAT.hpp"
#include "PartitionSchemes/BootProtection.hpp"
#include "PartitionTypes/Helpers.hpp"
#include "Utils/UTF.hpp"

#include <bits/chrono.h>
#include <cctype>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string_view>

namespace FAT
{
	int CompareFilenames(std::u32string_view lhs, std::u32string_view rhs)
	{
		if (lhs.size() < rhs.size())
			return -1;
		if (lhs.size() > rhs.size())
			return 1;

		for (size_t i = 0; i < lhs.size(); ++i)
		{
			char32_t lhsc = UTF::UTF32ToLower(lhs[i]);
			char32_t rhsc = UTF::UTF32ToLower(rhs[i]);
			if (lhsc < rhsc)
				return -1;
			if (lhsc > rhsc)
				return 1;
		}
		return 0;
	}

	uint16_t MakeDate(uint8_t day, uint8_t month, uint16_t year)
	{
		day   = day < 1 ? 1 : (day > 31 ? 31 : day);
		month = month < 1 ? 1 : (month > 12 ? 12 : month);
		year  = year < 1980 ? 1980 : (year > 2107 ? 2107 : year);
		year -= 1980;
		return year << 9 | month << 5 | day;
	}

	uint16_t MakeTime(uint8_t hour, uint8_t minute, uint8_t second)
	{
		hour     = hour > 23 ? 23 : hour;
		minute   = minute > 59 ? 59 : minute;
		second   = second > 58 ? 58 : second;
		second >>= 1;
		return hour << 11 | minute << 5 | second;
	}

	void GetDate(uint16_t date, uint8_t& day, uint8_t& month, uint16_t& year)
	{
		day   = date & 31;
		month = (date >> 5) & 15;
		year  = date >> 9;
	}

	void GetTime(uint16_t time, uint8_t& hour, uint8_t& minute, uint8_t& second)
	{
		hour   = time >> 11;
		minute = (time >> 5) & 63;
		second = (time & 31) << 1;
	}

	uint16_t Today()
	{
		auto                        now = std::chrono::system_clock::now();
		std::chrono::year_month_day ymd(std::chrono::floor<std::chrono::days>(now));
		return MakeDate((unsigned) ymd.day(), (unsigned) ymd.month(), (int) ymd.year());
	}

	uint16_t Now()
	{
		auto now = std::chrono::system_clock::now();
		auto hms = std::chrono::hh_mm_ss(now - std::chrono::floor<std::chrono::days>(now));
		return MakeTime((uint8_t) hms.hours().count(), (uint8_t) hms.minutes().count(), (uint8_t) hms.seconds().count());
	}

	uint32_t AllocCluster(State& state)
	{
		uint32_t curCluster = state.NextFreeCluster;
		do
		{
			uint32_t value = state.GetFAT(curCluster);
			if (value == 0U)
			{
				--state.FreeClusters;
				state.NextFreeCluster = (curCluster + 1) % state.MaxClusters;
				state.SetFATEnd(curCluster);
				return curCluster;
			}
			curCluster = (curCluster + 1) % state.MaxClusters;
		}
		while (curCluster != state.NextFreeCluster);
		return 0;
	}

	void FreeCluster(State& state, uint32_t cluster)
	{
		state.SetFAT(cluster, 0);
		++state.FreeClusters;
	}

	uint32_t AllocClusters(State& state, uint32_t count)
	{
		if (count == 0)
			return 0;
		uint32_t firstCluster = AllocCluster(state);
		if (!firstCluster)
			return 0;
		uint32_t curCluster = firstCluster;
		for (uint32_t i = 0; i < count; ++i)
		{
			uint32_t nextCluster = AllocCluster(state);
			state.SetFAT(curCluster, nextCluster);
			curCluster = nextCluster;
		}
		state.SetFATEnd(curCluster);
		return firstCluster;
	}

	void FreeClusters(State& state, uint32_t firstCluster)
	{
		if (!firstCluster)
			return;

		while (!state.IsFATEnd(firstCluster))
		{
			uint32_t nextCluster = state.GetFAT(firstCluster);
			FreeCluster(state, firstCluster);
			firstCluster = nextCluster;
		}
	}

	uint32_t NextCluster(State& state, uint32_t cluster, bool alloc = false)
	{
		if (!cluster)
			return 0;

		if (state.IsFATEnd(cluster))
		{
			if (!alloc)
				return 0;

			uint32_t nextCluster = AllocCluster(state);
			if (!nextCluster)
				return 0;
			state.SetFAT(cluster, nextCluster);
			state.SetFATEnd(nextCluster);
			return nextCluster;
		}
		else
		{
			return state.GetFAT(cluster);
		}
	}

	uint32_t ReadCluster(State& state, void* buffer, uint32_t cluster, bool alloc = false)
	{
		if (!cluster)
			return 0;

		for (size_t i = 0; i < state.ClusterSize; ++i)
			PartHelpers::ReadSector(*state.Partition, *state.FStream, (uint8_t*) buffer + i * 512, state.FirstClusterSector + cluster * state.ClusterSize + i);
		return NextCluster(state, cluster, alloc);
	}

	uint32_t WriteCluster(State& state, const void* buffer, uint32_t cluster, bool alloc = false)
	{
		if (!cluster)
			return 0;

		for (size_t i = 0; i < state.ClusterSize; ++i)
			PartHelpers::WriteSector(*state.Partition, *state.FStream, (const uint8_t*) buffer + i * 512, state.FirstClusterSector + cluster * state.ClusterSize + i);
		return NextCluster(state, cluster, alloc);
	}

	size_t ReadClusters(State& state, void* buffer, size_t size, uint32_t firstCluster, bool alloc = false)
	{
		size_t count = 0;
		while (firstCluster && size)
		{
			firstCluster = ReadCluster(state, (uint8_t*) buffer + count * state.ClusterSize, firstCluster, alloc);
			--size;
			++count;
		}
		return count;
	}

	size_t WriteClusters(State& state, const void* buffer, size_t size, uint32_t firstCluster, bool alloc = false)
	{
		size_t count = 0;
		while (firstCluster && size)
		{
			firstCluster = WriteCluster(state, (const uint8_t*) buffer + count * state.ClusterSize, firstCluster, alloc);
			--size;
			++count;
		}
		return count;
	}

	void ParseDirectoryEntries(DirectoryState& directoryState)
	{
		directoryState.ParsedEntries.clear();
		char16_t utf16FilenameBuf[261];
		uint8_t  chcksums[20];
		uint8_t  count = 0;
		memset(utf16FilenameBuf, 0, sizeof(utf16FilenameBuf));
		memset(chcksums, 0, sizeof(chcksums));
		for (size_t i = 0; i < directoryState.Entries.size(); ++i)
		{
			auto& entr = directoryState.Entries[i];
			if (entr.Attr == 0x00)
				continue;

			bool     invalid    = false;
			uint32_t firstEntry = (uint32_t) i;
			for (count = 0; count < 20 && (directoryState.Entries[i].Attr & ATTR_LDIR) == ATTR_LDIR; ++i, ++count)
			{
				LDIR* ldir = (LDIR*) &directoryState.Entries[i];
				if (count > 0 && ldir->Ord & 0x40)
				{
					std::cerr << "ImgGen ERROR: FAT Long filename entry '" << firstEntry << "' ended with another Long filename '" << i << "' in cluster '" << directoryState.FirstCluster << "'\n";
					--i;
					invalid = true;
					break;
				}
				size_t index = ((ldir->Ord & 0x3F) - 1) * 13;
				memcpy(utf16FilenameBuf + index, ldir->Name1, 10);
				memcpy(utf16FilenameBuf + index + 5, ldir->Name2, 12);
				memcpy(utf16FilenameBuf + index + 11, ldir->Name3, 4);
				if (ldir->Ord & 0x40)
					utf16FilenameBuf[index + 13] = u'\0';
				chcksums[(ldir->Ord & 0x3F) - 1] = ldir->Chcksum;
			}
			if (invalid)
				continue;
			uint32_t lastEntry = (uint32_t) i;

			auto& entry = directoryState.Entries[i];
			if ((entry.Attr & ATTR_LDIR) == ATTR_LDIR)
			{
				std::cerr << "ImgGen ERROR: FAT Directory entry '" << i << "' in cluster '" << directoryState.FirstCluster << "' has too many LDIR entries";
				--i;
				continue;
			}

			std::u32string fullName;
			if (count > 0)
			{
				uint8_t checksum = 0;
				for (size_t i = 0; i < 11; ++i)
					checksum = ((checksum << 7) | (checksum >> 1)) + entry.Name[i];
				for (size_t i = 0; i < count; ++i)
				{
					if (chcksums[i] != checksum)
					{
						std::cerr << "ImgGen ERROR: FAT Directory entry '" << i << "' in cluster '" << directoryState.FirstCluster << "' has Long filename entry '" << lastEntry - i - 1 << "' with incorrect checksum, will fall back to short name\n";
						invalid = true;
					}
				}
				if (invalid)
				{
					fullName.resize(12);
					for (size_t i = 0; i < 8; ++i)
						fullName[i] = (entry.NTRes >> 3) & 1 ? std::tolower(entry.Name[i]) : entry.Name[i];
					size_t j    = fullName.find_last_not_of(' ') + 1;
					fullName[j] = U'.';
					for (size_t i = j + 1, k = 0; k < 3; ++i, ++k)
						fullName[i] = (entry.NTRes >> 4) & 1 ? std::tolower(entry.Name[8 + k]) : entry.Name[8 + k];
					fullName.resize(fullName.find_last_not_of(' '));
				}
				else
				{
					fullName = UTF::UTF16ToUTF32(utf16FilenameBuf);
				}
			}
			else
			{
				fullName.resize(12);
				for (size_t i = 0; i < 8; ++i)
					fullName[i] = (entry.NTRes >> 3) & 1 ? std::tolower(entry.Name[i]) : entry.Name[i];
				size_t j    = fullName.find_last_not_of(' ') + 1;
				fullName[j] = U'.';
				for (size_t i = j + 1, k = 0; k < 3; ++i, ++k)
					fullName[i] = (entry.NTRes >> 4) & 1 ? std::tolower(entry.Name[8 + k]) : entry.Name[8 + k];
				fullName.resize(fullName.find_last_not_of(' '));
			}

			auto& parsedEntry      = directoryState.ParsedEntries.emplace_back();
			parsedEntry.FirstEntry = firstEntry;
			parsedEntry.Entry      = lastEntry;
			parsedEntry.Cluster    = entry.FstClusHI << 16 | entry.FstClusLO;
			parsedEntry.FileSize   = entry.FileSize;
			parsedEntry.FullName   = fullName;
			memcpy(parsedEntry.ShortName, entry.Name, 11);
			parsedEntry.CreateDate = entry.CrtDate;
			parsedEntry.CreateTime = entry.CrtTime;
			parsedEntry.WriteDate  = entry.WrtDate;
			parsedEntry.WriteTime  = entry.WrtTime;
			parsedEntry.AccessDate = entry.LstAccDate;
			parsedEntry.Attribute  = entry.Attr;
		}
	}

	DirectoryState LoadDirectoryState(State& state, uint32_t cluster)
	{
		DirectoryState directoryState;
		directoryState.State        = &state;
		directoryState.Parent       = 0;
		directoryState.FirstCluster = cluster;
		directoryState.LastCluster  = cluster;
		directoryState.Entries.resize(512 * state.ClusterSize / sizeof(DIR));
		size_t   offset     = 0;
		uint32_t curCluster = cluster;
		while (curCluster != 0)
		{
			directoryState.Entries.resize(directoryState.Entries.size() + 512 * state.ClusterSize / sizeof(DIR));
			directoryState.LastCluster = curCluster;
			curCluster                 = ReadCluster(state, (uint8_t*) directoryState.Entries.data() + offset, curCluster);
			offset                    += 512 * state.ClusterSize;
		}
		ParseDirectoryEntries(directoryState);
		return directoryState;
	}

	void FreeDirectoryEntryClusters(State& State, uint32_t directoryCluster)
	{
		DirectoryState directoryState = LoadDirectoryState(State, directoryCluster);
		for (auto& entry : directoryState.ParsedEntries)
		{
			if (entry.Attribute & ATTR_DIRECTORY)
				FreeDirectoryEntryClusters(State, entry.Cluster);
			else
				FreeClusters(State, entry.Cluster);
		}
		FreeClusters(State, directoryCluster);
	}

	bool DirectoryState::FillShortName(std::u32string_view fullName, char (&shortName)[11], bool& lowercaseFilename, bool& lowercaseExtension)
	{
		bool   needsLongName      = false;
		size_t lowercaseNameCount = 0;
		size_t lowercaseExtCount  = 0;

		size_t extBegin = std::min<size_t>(fullName.size(), fullName.find_last_of('.'));
		if (extBegin > 8 || (fullName.size() - extBegin) > 4)
			needsLongName = true;

		memset(shortName, ' ', 11);
		for (size_t i = 0; i < std::min<size_t>(8, extBegin); ++i)
		{
			char32_t codepoint = fullName[i];
			if (codepoint >= 0x80)
			{
				needsLongName = true;
				shortName[i]  = '_';
			}
			else
			{
				shortName[i]        = std::toupper((char) codepoint);
				lowercaseNameCount += codepoint >= 'a' && codepoint <= 'z';
			}
		}
		for (size_t i = extBegin + 1, j = 8; i < fullName.size() && j < 3; ++i, ++j)
		{
			char32_t codepoint = fullName[i];
			if (codepoint >= 0x80)
			{
				needsLongName = true;
				shortName[j]  = '_';
			}
			else
			{
				shortName[i]       = std::toupper((char) codepoint);
				lowercaseExtCount += codepoint >= 'a' && codepoint <= 'z';
			}
		}
		if (!needsLongName)
		{
			lowercaseFilename  = lowercaseNameCount == std::min<size_t>(8, extBegin);
			lowercaseExtension = lowercaseExtCount == std::min<size_t>(3, fullName.size() - extBegin - 1);
		}
		else
		{
			lowercaseFilename  = false;
			lowercaseExtension = false;
		}
		return needsLongName;
	}

	void DirectoryState::GenShortNameTail(char (&shortName)[11])
	{
		uint32_t nextN = 1;
		for (auto& entry : ParsedEntries)
		{
			if (memcmp(entry.ShortName + 8, shortName + 8, 3) != 0) // Different extensions
				continue;

			bool   foundMatch = true;
			size_t i          = 0;
			for (; i < 8; ++i)
			{
				char c = entry.ShortName[i];
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

			uint64_t v = std::strtoull((const char*) entry.ShortName + i, nullptr, 10);
			if (v > nextN)
				nextN = (uint32_t) std::min<uint64_t>(v, 1'000'000) + 1;
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

	DIR& DirectoryState::NewRawEntry()
	{
		if (Entries.size() % 512 == 0)
			LastCluster = NextCluster(*State, LastCluster, true);
		return Entries.emplace_back();
	}

	void DirectoryState::AddLDIREntries(std::u32string_view fullName, uint8_t shortNameChecksum)
	{
		std::u16string utf16FullName = UTF::UTF32ToUTF16(fullName);
		uint32_t       entryCount    = (utf16FullName.size() + 12) / 13;
		for (uint32_t i = entryCount, j = 0; i-- > 0; ++j)
		{
			LDIR& entry = *(LDIR*) &NewRawEntry();
			entry.Ord   = (uint8_t) (i + 1);
			if (j == 0)
				entry.Ord |= 0x40;
			entry.Attr      = ATTR_LDIR;
			entry.Type      = 0;
			entry.Chcksum   = shortNameChecksum;
			entry.FstClusLO = 0;
			memset(entry.Name1, 0xFF, 10);
			memset(entry.Name2, 0xFF, 12);
			memset(entry.Name3, 0xFF, 4);
			size_t charsLeft = std::min<size_t>(13, utf16FullName.size() - i * 13);
			for (size_t k = 0; k < charsLeft; ++k)
			{
				if (k < 5)
					entry.Name1[k] = utf16FullName[i * 13 + k];
				else if (k < 11)
					entry.Name2[k - 5] = utf16FullName[i * 13 + k];
				else
					entry.Name3[k - 11] = utf16FullName[i * 13 + k];
			}
			if (charsLeft < 5)
				entry.Name1[charsLeft] = u'\0';
			else if (charsLeft < 11)
				entry.Name2[charsLeft - 5] = u'\0';
			else if (charsLeft < 13)
				entry.Name3[charsLeft - 11] = u'\0';
		}
	}

	DirectoryEntry* DirectoryState::AddEntry(std::u32string_view fullName)
	{
		char shortName[11];
		bool lowercaseFilename  = false;
		bool lowercaseExtension = false;
		bool needsLongName      = FillShortName(fullName, shortName, lowercaseFilename, lowercaseExtension);

		uint32_t firstEntry = Entries.size();
		if (needsLongName)
		{
			GenShortNameTail(shortName);

			uint8_t checksum = 0;
			for (size_t i = 0; i < 11; ++i)
				checksum = ((checksum << 7) | (checksum >> 1)) + shortName[i];
			AddLDIREntries(fullName, checksum);
		}

		uint16_t today = Today();
		uint16_t now   = Now();

		DIR& entry = NewRawEntry();
		memcpy(entry.Name, shortName, 11);
		entry.Attr         = 0x00;
		entry.NTRes        = lowercaseFilename << 3 | lowercaseExtension << 4;
		entry.CrtTimeTenth = 0;
		entry.CrtTime      = now;
		entry.CrtDate      = today;
		entry.LstAccDate   = today;
		entry.FstClusHI    = 0;
		entry.WrtTime      = now;
		entry.WrtDate      = today;
		entry.FstClusLO    = 0;
		entry.FileSize     = 0;
		uint32_t lastEntry = Entries.size();

		auto& parsedEntry      = ParsedEntries.emplace_back();
		parsedEntry.FirstEntry = firstEntry;
		parsedEntry.Entry      = lastEntry;
		parsedEntry.Cluster    = 0;
		parsedEntry.FileSize   = 0;
		parsedEntry.FullName   = fullName;
		memcpy(parsedEntry.ShortName, shortName, 11);
		parsedEntry.CreateDate = today;
		parsedEntry.CreateTime = now;
		parsedEntry.WriteDate  = today;
		parsedEntry.WriteTime  = now;
		parsedEntry.AccessDate = today;
		parsedEntry.Attribute  = 0x00;
		return &parsedEntry;
	}

	DirectoryEntry* DirectoryState::GetEntry(std::u32string_view fullName)
	{
		for (size_t i = 0; i < ParsedEntries.size(); ++i)
		{
			if (CompareFilenames(ParsedEntries[i].FullName, fullName) == 0)
				return &ParsedEntries[i];
		}
		return nullptr;
	}

	DirectoryEntry* DirectoryState::RenameEntry(std::u32string_view oldFullName, std::u32string_view newFullName)
	{
		DirectoryEntry copiedParsedEntry;
		{
			size_t i = 0;
			for (; i < ParsedEntries.size(); ++i)
			{
				if (CompareFilenames(ParsedEntries[i].FullName, oldFullName) == 0)
					break;
			}
			if (i == ParsedEntries.size())
				return nullptr;

			auto& parsedEntry = ParsedEntries[i];
			Entries.erase(Entries.begin() + parsedEntry.FirstEntry, Entries.end() + parsedEntry.Entry + 1);
			if (parsedEntry.Attribute & ATTR_DIRECTORY)
				FreeDirectoryEntryClusters(*State, parsedEntry.Cluster);
			else
				FreeClusters(*State, parsedEntry.Cluster);

			uint32_t delta    = 1 + parsedEntry.Entry - parsedEntry.FirstEntry;
			copiedParsedEntry = std::move(parsedEntry);
			ParsedEntries.erase(ParsedEntries.begin() + i);

			for (; i < ParsedEntries.size(); ++i)
			{
				ParsedEntries[i].FirstEntry -= delta;
				ParsedEntries[i].Entry      -= delta;
			}
		}

		char shortName[11];
		bool lowercaseFilename  = false;
		bool lowercaseExtension = false;
		bool needsLongName      = FillShortName(newFullName, shortName, lowercaseFilename, lowercaseExtension);

		uint32_t firstEntry = Entries.size();
		if (needsLongName)
		{
			GenShortNameTail(shortName);

			uint8_t checksum = 0;
			for (size_t i = 0; i < 11; ++i)
				checksum = ((checksum << 7) | (checksum >> 1)) + shortName[i];
			AddLDIREntries(newFullName, checksum);
		}

		uint16_t today = Today();
		uint16_t now   = Now();

		DIR& entry = NewRawEntry();
		memcpy(entry.Name, shortName, 11);
		entry.Attr         = copiedParsedEntry.Attribute;
		entry.NTRes        = lowercaseFilename << 3 | lowercaseExtension << 4;
		entry.CrtTimeTenth = 0;
		entry.CrtTime      = copiedParsedEntry.CreateTime;
		entry.CrtDate      = copiedParsedEntry.CreateDate;
		entry.LstAccDate   = today;
		entry.FstClusHI    = copiedParsedEntry.Cluster >> 16;
		entry.WrtTime      = now;
		entry.WrtDate      = today;
		entry.FstClusLO    = (uint16_t) copiedParsedEntry.Cluster;
		entry.FileSize     = copiedParsedEntry.FileSize;
		uint32_t lastEntry = Entries.size();

		auto& parsedEntry      = ParsedEntries.emplace_back();
		parsedEntry            = std::move(copiedParsedEntry);
		parsedEntry.FirstEntry = firstEntry;
		parsedEntry.Entry      = lastEntry;
		parsedEntry.FullName   = newFullName;
		memcpy(parsedEntry.ShortName, shortName, 11);
		parsedEntry.WriteDate  = today;
		parsedEntry.WriteTime  = now;
		parsedEntry.AccessDate = today;
		return &parsedEntry;
	}

	void DirectoryState::RemoveEntry(std::u32string_view fullName)
	{
		size_t i = 0;
		for (; i < ParsedEntries.size(); ++i)
		{
			if (CompareFilenames(ParsedEntries[i].FullName, fullName) == 0)
				break;
		}
		if (i == ParsedEntries.size())
			return;

		auto& parsedEntry = ParsedEntries[i];
		Entries.erase(Entries.begin() + parsedEntry.FirstEntry, Entries.end() + parsedEntry.Entry + 1);
		if (parsedEntry.Attribute & ATTR_DIRECTORY)
			FreeDirectoryEntryClusters(*State, parsedEntry.Cluster);
		else
			FreeClusters(*State, parsedEntry.Cluster);

		uint32_t delta = 1 + parsedEntry.Entry - parsedEntry.FirstEntry;
		ParsedEntries.erase(ParsedEntries.begin() + i);

		for (; i < ParsedEntries.size(); ++i)
		{
			ParsedEntries[i].FirstEntry -= delta;
			ParsedEntries[i].Entry      -= delta;
		}
	}

	void DirectoryState::NewDirectory(std::u32string_view fullName)
	{
		auto  parsedEntry      = AddEntry(fullName);
		auto& entry            = Entries[parsedEntry->Entry];
		parsedEntry->Attribute = ATTR_DIRECTORY;
		parsedEntry->Cluster   = AllocCluster(*State);
		entry.Attr             = ATTR_DIRECTORY;
		entry.FstClusHI        = parsedEntry->Cluster >> 16;
		entry.FstClusLO        = (uint16_t) parsedEntry->Cluster;

		
	}

	void DirectoryState::NewFile(std::u32string_view fullName)
	{
		auto  parsedEntry      = AddEntry(fullName);
		auto& entry            = Entries[parsedEntry->Entry];
		parsedEntry->Attribute = ATTR_DIRECTORY;
		parsedEntry->Cluster   = AllocCluster(*State);
		entry.Attr             = ATTR_DIRECTORY;
		entry.FstClusHI        = parsedEntry->Cluster >> 16;
		entry.FstClusLO        = (uint16_t) parsedEntry->Cluster;
		entry.FileSize         = 0;
	}

	void State::SetFAT(uint32_t cluster, uint32_t nextCluster)
	{
		if (cluster >= MaxClusters)
			return;
		switch (Type)
		{
		case EFATType::FAT12:
		{
			uint8_t   el = cluster & 7;
			uint16_t* pV = (uint16_t*) ((uint8_t*) FAT[cluster >> 3].Values + el * 12 / 8);
			*pV          = *pV & ~(0xFFF << (4 * (el & 1))) | (nextCluster & 0xFFF) << (4 * (el & 1));
			break;
		}
		case EFATType::FAT16:
		{
			FATEntry& entry       = FAT[cluster / 6];
			uint8_t   el          = cluster % 6;
			entry.Values[el >> 1] = entry.Values[el >> 1] & ~(0xFFFF << (16 * (el & 1))) | ((nextCluster & 0xFFFF) << (16 * (el & 1)));
			break;
		}
		case EFATType::FAT32: FAT[cluster / 3].Values[cluster % 3] = nextCluster; break;
		}
	}

	void State::SetFATEnd(uint32_t cluster)
	{
		if (cluster >= MaxClusters)
			return;
		switch (Type)
		{
		case EFATType::FAT12:
		{
			uint8_t   el = cluster & 7;
			uint16_t* pV = (uint16_t*) ((uint8_t*) FAT[cluster >> 3].Values + el * 12 / 8);
			*pV          = *pV & ~(0xFFF << (4 * (el & 1))) | 0xFFF << (4 * (el & 1));
			break;
		}
		case EFATType::FAT16:
		{
			FATEntry& entry       = FAT[cluster / 6];
			uint8_t   el          = cluster % 6;
			entry.Values[el >> 1] = entry.Values[el >> 1] & ~(0xFFFF << (16 * (el & 1))) | (0xFFFF << (16 * (el & 1)));
			break;
		}
		case EFATType::FAT32: FAT[cluster / 3].Values[cluster % 3] = 0xFFFF'FFFFU; break;
		}
	}

	bool State::IsFATEnd(uint32_t cluster)
	{
		uint32_t next = GetFAT(cluster);
		switch (Type)
		{
		case EFATType::FAT12: return next == 0xFFF;
		case EFATType::FAT16: return next == 0xFFFF;
		case EFATType::FAT32: return next == 0xFFFF'FFFFU;
		default: return false;
		}
	}

	uint32_t State::GetFAT(uint32_t cluster)
	{
		if (cluster >= MaxClusters)
			return 0;
		switch (Type)
		{
		case EFATType::FAT12:
		{
			uint8_t el = cluster & 7;
			return (*(uint16_t*) ((uint8_t*) FAT[cluster >> 3].Values + el * 12 / 8) >> (4 * (el & 1))) & 0xFFF;
		}
		case EFATType::FAT16:
		{
			uint8_t el = cluster % 6;
			return (FAT[cluster / 6].Values[el >> 1] >> (16 * (el & 1))) & 0xFFFF;
		}
		case EFATType::FAT32: return FAT[cluster / 3].Values[cluster % 3];
		default: return 0;
		}
	}

	State LoadState(ImgGenOptions& options, PartitionOptions& partition, std::fstream& fstream)
	{
		State state;
		state.Options   = &options;
		state.Partition = &partition;
		state.FStream   = &fstream;

		do
		{
			if (partition.ForceReformat)
				break;

			uint8_t bpbBuf[512];
			FSInfo  fsInfo;
			memset(bpbBuf, 0, 512);
			memset(&fsInfo, 0, 512);

			EBPB16&  ebpb16         = *(EBPB16*) bpbBuf;
			EBPB32&  ebpb32         = *(EBPB32*) bpbBuf;
			BPB&     bpb            = static_cast<BPB&>(ebpb16);
			BPB32&   bpb32          = static_cast<BPB32&>(ebpb32);
			BS*      bs             = nullptr;
			uint8_t* bootCode       = nullptr;
			size_t   bootCodeLength = 0;

			PartHelpers::ReadSector(partition, fstream, bpbBuf, 0);

			if (bpb.BytsPerSec != 512)
			{
				std::cerr << "ImgGen ERROR: FAT partition contains unexpected bytes per sector value '" << bpb.BytsPerSec << "', will reformat\n";
				partition.ForceReformat = true;
				break;
			}

			uint32_t rootDirSectors  = (bpb.RootEntCnt * 32 + 511) / 512;
			state.ClusterSize        = bpb.SecPerClus;
			state.FATSize            = bpb.FATSz16 != 0 ? bpb.FATSz16 : bpb32.FATSz32; // INFO(MarcasRealAccount): This is some highly stupid code, why you do this microsoft!!!
			state.FATCount           = bpb.NumFATs;
			state.FirstFATSector     = bpb.RsvdSecCnt;
			state.FirstClusterSector = state.FirstFATSector + state.FATCount * state.FATSize + rootDirSectors;
			state.MaxClusters        = ((bpb.TotSec16 != 0 ? bpb.TotSec16 : bpb.TotSec32) - state.FirstClusterSector) / state.ClusterSize;

			if (state.MaxClusters < 4085)
				state.Type = EFATType::FAT12;
			else if (state.MaxClusters < 65525)
				state.Type = EFATType::FAT16;
			else
				state.Type = EFATType::FAT32;

			bool countFreeClusters = true;
			if (state.Type == EFATType::FAT32)
			{
				PartHelpers::ReadSector(partition, fstream, &fsInfo, bpb32.FSInfo);
				bs             = static_cast<BS*>(&ebpb32);
				bootCode       = ebpb32.BootCode;
				bootCodeLength = sizeof(ebpb32.BootCode);

				if (bpb32.ExtFlags & 0x80)
				{
					state.FATMirrored = false;
					state.CurFAT      = bpb32.ExtFlags & 0xF;
				}
				else
				{
					state.FATMirrored = true;
					state.CurFAT      = 0;
				}

				if (fsInfo.LeadSig == 0x41615252 &&
					fsInfo.StrucSig == 0x61417272 &&
					fsInfo.TrailSig == 0xAA550000)
				{
					state.FreeClusters    = fsInfo.Free_Count;
					state.NextFreeCluster = fsInfo.Nxt_Free;
					countFreeClusters     = false;
				}

				state.RootDir.Parent       = 0;
				state.RootDir.FirstCluster = bpb32.RootClus;
				state.RootDir.Entries.clear();
				uint32_t curCluster = state.RootDir.FirstCluster;
				size_t   offset     = 0;
				while (curCluster != 0)
				{
					state.RootDir.Entries.resize(state.RootDir.Entries.size() + 512 * state.ClusterSize / sizeof(DIR));
					curCluster = ReadCluster(state, (uint8_t*) state.RootDir.Entries.data() + offset, curCluster);
					offset    += 512 * state.ClusterSize;
				}
			}
			else
			{
				bs             = static_cast<BS*>(&ebpb16);
				bootCode       = ebpb16.BootCode;
				bootCodeLength = sizeof(ebpb16.BootCode);

				state.FATMirrored = true;
				state.CurFAT      = 0;

				state.RootDir.Parent       = 0;
				state.RootDir.FirstCluster = 0;
				state.RootDir.Entries.resize(bpb.RootEntCnt);
				for (size_t i = 0; i < rootDirSectors; ++i)
					PartHelpers::ReadSector(partition, fstream, (uint8_t*) state.RootDir.Entries.data() + i * 512, state.FirstFATSector + state.FATCount * state.FATSize + i);
			}
			state.FATSector = state.FirstFATSector + state.CurFAT * state.FATSize;

			state.FAT.resize((state.FATSize * 512 + sizeof(FATEntry) - 1) / sizeof(FATEntry));
			for (size_t i = 0; i < state.FATSize; ++i)
				PartHelpers::ReadSector(partition, fstream, (uint8_t*) state.FAT.data() + i * 512, state.FATSector + i);

			if (countFreeClusters)
			{
				state.FreeClusters    = 0;
				state.NextFreeCluster = ~0U;
				for (size_t i = 0; i < state.MaxClusters; ++i)
				{
					if (state.GetFAT((uint32_t) i) == 0)
					{
						if (state.NextFreeCluster == ~0U)
							state.NextFreeCluster = (uint32_t) i;
						++state.FreeClusters;
					}
				}

				if (state.Type == EFATType::FAT32)
				{
					fsInfo.LeadSig    = 0x41615252;
					fsInfo.StrucSig   = 0x61417272;
					fsInfo.Free_Count = state.FreeClusters;
					fsInfo.Nxt_Free   = state.NextFreeCluster;
					fsInfo.TrailSig   = 0xAA550000;
					PartHelpers::WriteSector(partition, fstream, &fsInfo, bpb32.FSInfo);
					PartHelpers::WriteSector(partition, fstream, &fsInfo, bpb32.BkBootSec + bpb32.FSInfo);
				}
			}
		}
		while (false);

		if (partition.ForceReformat)
		{
			uint8_t bpbBuf[512];
			FSInfo  fsInfo;
			memset(bpbBuf, 0, 512);
			memset(&fsInfo, 0, 512);

			EBPB16&  ebpb16         = *(EBPB16*) bpbBuf;
			EBPB32&  ebpb32         = *(EBPB32*) bpbBuf;
			BPB&     bpb            = static_cast<BPB&>(ebpb16);
			BPB32&   bpb32          = static_cast<BPB32&>(ebpb32);
			BS*      bs             = nullptr;
			uint8_t* bootCode       = nullptr;
			size_t   bootCodeLength = 0;

			memcpy(bpb.JmpBoot, "\xEB\x00\x90", 3);
			memcpy(bpb.OEMName, "MSWIN4.1", 8);
			bpb.BytsPerSec        = 512;
			bpb.SecPerClus        = 8;
			bpb.NumFATs           = 2;
			bpb.Media             = 0xF8;
			bpb.SecPerTrk         = 0x3F;
			bpb.NumHeads          = 0xFF;
			bpb.HiddSec           = (uint32_t) std::min<uint64_t>(partition.ActualStart, 0xFFFF'FFFF);
			ebpb32.Signature_word = 0xAA55;

			uint32_t sectorCount = (uint32_t) (partition.ActualEnd - partition.ActualStart);
			if (sectorCount < 8192)
				state.Type = EFATType::FAT12;
			else if (sectorCount < 1048576)
				state.Type = EFATType::FAT16;
			else
				state.Type = EFATType::FAT32;

			if (state.Type == EFATType::FAT32)
			{
				bs             = static_cast<BS*>(&ebpb32);
				bootCode       = ebpb32.BootCode;
				bootCodeLength = sizeof(ebpb32.BootCode);
				bpb.RsvdSecCnt = 32;
				bpb.RootEntCnt = 0;
				bpb.TotSec16   = 0;
				bpb.TotSec32   = sectorCount;
				memcpy(bs->FilSysType, "FAT32   ", 8);
			}
			else
			{
				bs             = static_cast<BS*>(&ebpb16);
				bootCode       = ebpb16.BootCode;
				bootCodeLength = sizeof(ebpb16.BootCode);
				bpb.RsvdSecCnt = 1;
				bpb.RootEntCnt = 512;
				bpb.TotSec16   = (uint16_t) sectorCount;
				bpb.TotSec32   = 0;
				memcpy(bs->FilSysType, state.Type == EFATType::FAT16 ? "FAT16   " : "FAT12   ", 8);
			}

			memcpy(bootCode, c_LegacyBootProtection, std::min<size_t>(bootCodeLength, 512)); // min not needed, but just to be safe

			bs->DrvNum  = 0x80;
			bs->BootSig = 0x29;
			bs->VolID   = Today() << 16 | Now();
			memcpy(bs->VolLab, partition.Name.c_str(), std::min<size_t>(11, partition.Name.size()));

			bpb.JmpBoot[1] = bootCode - bpbBuf;

			state.ClusterSize       = 8;
			state.FATCount          = 2;
			uint32_t rootDirSectors = (bpb.RootEntCnt * 32 + 511) / 512;
			uint32_t tempVal        = 256 * state.ClusterSize + state.FATCount;
			if (state.Type == EFATType::FAT32)
				tempVal /= 2;
			state.FATSize            = sectorCount - bpb.RsvdSecCnt - rootDirSectors;
			state.FirstFATSector     = bpb.RsvdSecCnt;
			state.FATMirrored        = true;
			state.CurFAT             = 0;
			state.FATSector          = state.FirstFATSector;
			state.FirstClusterSector = state.FATSector + state.FATCount * state.FATSize + rootDirSectors;
			state.MaxClusters        = (sectorCount - state.FirstClusterSector) / state.ClusterSize;
			state.FreeClusters       = state.MaxClusters;
			state.NextFreeCluster    = 2;

			if (state.Type == EFATType::FAT32)
			{
				state.RootDir.Parent       = 0;
				state.RootDir.FirstCluster = AllocCluster(state);
				state.RootDir.Entries.resize((state.ClusterSize * 512) / sizeof(DIR));

				bpb.FATSz16     = 0;
				bpb32.FATSz32   = state.FATSize;
				bpb32.ExtFlags  = 0x0000; // Mirror
				bpb32.FSVer     = 0x0000;
				bpb32.RootClus  = state.RootDir.FirstCluster;
				bpb32.FSInfo    = 1;
				bpb32.BkBootSec = 6;

				fsInfo.LeadSig    = 0x41615252;
				fsInfo.StrucSig   = 0x61417272;
				fsInfo.Free_Count = state.FreeClusters;
				fsInfo.Nxt_Free   = state.NextFreeCluster;
				fsInfo.TrailSig   = 0xAA550000;
			}
			else
			{
				state.RootDir.Parent       = 0;
				state.RootDir.FirstCluster = 0;
				state.RootDir.Entries.resize(bpb.RootEntCnt);

				bpb.FATSz16 = (uint16_t) state.FATSize;
			}

			PartHelpers::WriteSector(partition, fstream, bpbBuf, 0);

			if (state.Type == EFATType::FAT32)
			{
				PartHelpers::WriteSector(partition, fstream, &fsInfo, 1);
				PartHelpers::WriteSector(partition, fstream, bpbBuf, 6);
				PartHelpers::WriteSector(partition, fstream, &fsInfo, 7);
			}
		}
		return state;
	}

	void SaveState(State& state)
	{
		uint8_t bpbBuf[512];
		FSInfo  fsInfo;
		memset(bpbBuf, 0, 512);
		memset(&fsInfo, 0, 512);
		PartHelpers::ReadSector(*state.Partition, *state.FStream, bpbBuf, 0);

		EBPB16&  ebpb16         = *(EBPB16*) bpbBuf;
		EBPB32&  ebpb32         = *(EBPB32*) bpbBuf;
		BPB&     bpb            = static_cast<BPB&>(ebpb16);
		BPB32&   bpb32          = static_cast<BPB32&>(ebpb32);
		BS*      bs             = nullptr;
		uint8_t* bootCode       = nullptr;
		size_t   bootCodeLength = 0;

		if (state.Type == EFATType::FAT32)
		{
			bs             = static_cast<BS*>(&ebpb32);
			bootCode       = ebpb32.BootCode;
			bootCodeLength = sizeof(ebpb32.BootCode);

			bpb32.ExtFlags = state.FATMirrored ? state.CurFAT : 0x80;

			fsInfo.Free_Count = state.FreeClusters;
			fsInfo.Nxt_Free   = state.NextFreeCluster;

			PartHelpers::WriteSector(*state.Partition, *state.FStream, &fsInfo, bpb32.FSInfo);
			PartHelpers::WriteSector(*state.Partition, *state.FStream, &fsInfo, bpb32.BkBootSec + bpb32.FSInfo);

			WriteClusters(state, state.RootDir.Entries.data(), state.RootDir.Entries.size() * sizeof(DIR) / (512 * state.ClusterSize), state.RootDir.FirstCluster);
		}
		else
		{
			bs             = static_cast<BS*>(&ebpb16);
			bootCode       = ebpb16.BootCode;
			bootCodeLength = sizeof(ebpb16.BootCode);

			uint32_t rootDirSector = state.FirstFATSector + state.FATCount * state.FATSize;
			for (size_t i = 0; i < state.RootDir.Entries.size() * sizeof(DIR) / 512; ++i)
				PartHelpers::WriteSector(*state.Partition, *state.FStream, (const uint8_t*) state.RootDir.Entries.data() + i * 512, rootDirSector + i);
		}

		memcpy(bs->VolLab, state.Partition->Name.c_str(), std::min<size_t>(11, state.Partition->Name.size()));

		if (!state.Options->RetainBootCode)
		{
			memcpy(bpb.JmpBoot, "\xEB\x00\x90", 3);
			bpb.JmpBoot[1] = bootCode - bpbBuf;
			memcpy(bootCode, c_LegacyBootProtection, std::min<size_t>(bootCodeLength, 512)); // min not needed, but just to be safe
		}

		if (state.FATMirrored)
		{
			for (size_t i = 0; i < state.FATCount; ++i)
			{
				for (size_t j = 0; j < state.FATSize; ++j)
					PartHelpers::WriteSector(*state.Partition, *state.FStream, (const uint8_t*) state.FAT.data(), (state.FirstFATSector + i) * state.FATSize + j);
			}
		}
		else
		{
			for (size_t j = 0; j < state.FATSize; ++j)
				PartHelpers::WriteSector(*state.Partition, *state.FStream, (const uint8_t*) state.FAT.data(), state.FATSector + j);
		}

		for (auto& [path, dir] : state.SubDirectories)
			WriteClusters(state, dir.Entries.data(), dir.Entries.size() * sizeof(DIR) / (state.ClusterSize * 512), dir.FirstCluster);

		PartHelpers::WriteSector(*state.Partition, *state.FStream, bpbBuf, 0);
		if (state.Type == EFATType::FAT32)
			PartHelpers::WriteSector(*state.Partition, *state.FStream, bpbBuf, bpb32.BkBootSec);
	}

	uint64_t MaxFileSize(State& state)
	{
		return 0xFFFF'FFFF;
	}

	bool Exists(State& state, std::string_view path)
	{
		if (path.empty() || path[0] != '/')
			return false;
	}

	EFileType GetType(State& state, std::string_view path)
	{
		if (path.empty() || path[0] != '/')
			return EFileType::Invalid;
	}

	FileState GetFile(State& state, std::string_view path)
	{
	}

	FileState CreateFile(State& state, std::string_view path)
	{
	}

	void CloseFile(FileState& fileState)
	{
	}

	void EnsureFileSize(FileState& fileState, uint64_t fileSize)
	{
	}

	uint64_t GetFileSize(FileState& fileState)
	{
	}

	void SetModifyTime(FileState& fileState, uint64_t time)
	{
	}

	uint64_t GetModifyTime(FileState& fileState)
	{
	}

	uint64_t Read(FileState& fileState, void* buffer, size_t count)
	{
	}

	uint64_t Write(FileState& fileState, const void* buffer, size_t count)
	{
	}

	uint64_t GetMinPartitionSize()
	{
		return 8ULL * 512 * 8192;
	}

	uint64_t GetMaxPartitionSize()
	{
		return 8ULL * 512 * 0x0FFF'FFF7;
	}

	std::string Normalize(std::string_view path)
	{
	}
} // namespace FAT