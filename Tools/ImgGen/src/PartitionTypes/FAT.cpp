#include "FAT.hpp"
#include "PartitionSchemes/BootProtection.hpp"
#include "PartitionTypes/Helpers.hpp"
#include "State.hpp"
#include "Utils/UTF.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>

namespace FAT
{
	uint16_t ToFATDate(uint8_t day, uint8_t month, uint16_t year)
	{
		day   = day < 1 ? 1 : (day > 31 ? 31 : day);
		month = month < 1 ? 1 : (month > 12 ? 12 : month);
		year  = year < 1980 ? 1980 : (year > 2107 ? 2107 : year);
		year -= 1980;
		return (year << 9) | (month << 5) | day;
	}

	uint16_t ToFATTime(uint8_t hour, uint8_t minute, uint8_t second)
	{
		hour     = hour > 23 ? 23 : hour;
		minute   = minute > 59 ? 59 : minute;
		second   = second > 58 ? 58 : second;
		second >>= 1;
		return (hour << 11) | (minute << 5) | second;
	}

	void FromFATDate(uint16_t date, uint8_t& day, uint8_t& month, uint16_t& year)
	{
		day   = date & 31;
		month = (date >> 5) & 15;
		year  = (date >> 9) + 1980;
	}

	void FromFATTime(uint16_t time, uint8_t& hour, uint8_t& minute, uint8_t& second)
	{
		hour   = time >> 11;
		minute = (time >> 5) & 63;
		second = (time & 31) << 1;
	}

	void FATNow(uint16_t& date, uint16_t& time, uint8_t& timeTenth)
	{
		TimePoint now   = Clock::now();
		auto      today = std::chrono::floor<std::chrono::days>(now);
		auto      ymd   = std::chrono::year_month_day(today);
		auto      hms   = std::chrono::hh_mm_ss(now - today);
		date            = ToFATDate((unsigned) ymd.day(), (unsigned) ymd.month(), (int) ymd.year());
		time            = ToFATTime(hms.hours().count(), hms.minutes().count(), hms.seconds().count());
		timeTenth       = (hms.seconds().count() & 1) * 100; // TODO(MarcasRealAccount): Do proper tenths
	}

	bool DirectoryState::CompareFilenames(std::u32string_view lhs, std::u32string_view rhs)
	{
		if (lhs.size() != rhs.size())
			return false;
		for (size_t i = 0; i < lhs.size(); ++i)
		{
			if (UTF::UTF32ToLower(lhs[i]) != UTF::UTF32ToLower(rhs[i]))
				return false;
		}
		return true;
	}

	bool DirectoryState::FillShortName(std::u32string_view filename, char (&shortName)[11], bool& lowercaseFilename, bool& lowercaseExtension)
	{
		if (filename == U".")
		{
			memcpy(shortName, ".          ", 11);
			lowercaseFilename  = false;
			lowercaseExtension = false;
			return false;
		}
		if (filename == U"..")
		{
			memcpy(shortName, "..         ", 11);
			lowercaseFilename  = false;
			lowercaseExtension = false;
			return false;
		}

		bool   needsLongName      = false;
		size_t lowercaseNameCount = 0;
		size_t lowercaseExtCount  = 0;

		size_t extBegin = std::min<size_t>(filename.size(), filename.find_last_of(U'.'));
		if (extBegin > 8 || (filename.size() - extBegin) > 4)
			needsLongName = true;

		memset(shortName, ' ', 11);
		for (size_t i = 0; i < std::min<size_t>(8, extBegin); ++i)
		{
			char32_t codepoint = filename[i];
			if (codepoint >= 0x80)
			{
				needsLongName = true;
				shortName[i]  = '_';
			}
			else
			{
				shortName[i]        = std::toupper((char) codepoint);
				lowercaseNameCount += codepoint >= U'a' && codepoint <= U'z';
			}
		}
		for (size_t i = extBegin + 1, j = 8; i < filename.size() && j < 11; ++i, ++j)
		{
			char32_t codepoint = filename[i];
			if (codepoint >= 0x80)
			{
				needsLongName = true;
				shortName[j]  = '_';
			}
			else
			{
				shortName[j]       = std::toupper((char) codepoint);
				lowercaseExtCount += codepoint >= U'a' && codepoint <= U'z';
			}
		}
		if (needsLongName)
		{
			lowercaseFilename  = false;
			lowercaseExtension = false;
		}
		else
		{
			if (lowercaseNameCount == std::min<size_t>(8, extBegin))
				lowercaseFilename = true;
			else if (lowercaseNameCount == 0)
				lowercaseFilename = false;
			else
				needsLongName = true;

			if (lowercaseExtCount == std::min<size_t>(3, filename.size() - extBegin - 1))
				lowercaseExtension = true;
			else if (lowercaseExtCount == 0)
				lowercaseExtension = false;
			else
				needsLongName = true;
		}
		return needsLongName;
	}

	void DirectoryState::FillFilename(std::u32string& filename, const char (&shortName)[11], bool lowercaseFilename, bool lowercaseExtension)
	{
		if (memcmp(shortName, ".          ", 11) == 0)
		{
			filename = U".";
			return;
		}
		else if (memcmp(shortName, "..         ", 11) == 0)
		{
			filename = U"..";
			return;
		}

		size_t filenameEnd = 8;
		for (; filenameEnd-- > 0 && shortName[filenameEnd] == ' ';);
		++filenameEnd;
		size_t extEnd = 11;
		for (; extEnd-- > 8 && shortName[extEnd] == ' ';);
		++extEnd;
		filename.resize(1 + filenameEnd + extEnd - 8);
		size_t i = 0;
		for (size_t j = 0; j < filenameEnd; ++j)
			filename[i++] = (char32_t) (lowercaseFilename ? std::tolower(shortName[j]) : shortName[j]);
		filename[i++] = U'.';
		for (size_t j = 8; j < extEnd; ++j)
			filename[i++] = (char32_t) (lowercaseExtension ? std::tolower(shortName[j]) : shortName[j]);
	}

	uint8_t DirectoryState::ShortNameChecksum(char (&shortName)[11])
	{
		uint8_t checksum = 0;
		for (size_t i = 0; i < 11; ++i)
			checksum = ((checksum << 7) | (checksum >> 1)) + shortName[i];
		return checksum;
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

			uint64_t v = std::strtoull((const char*) entry.ShortName + i + 1, nullptr, 10);
			if (v >= nextN)
				nextN = (uint32_t) std::min<size_t>(v, 1'000'000) + 1;
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

	void DirectoryState::AddLDIREntries(std::u32string_view filename, uint8_t shortNameChecksum)
	{
		std::u16string utf16filename = UTF::UTF32ToUTF16(filename);
		uint32_t       entryCount    = (utf16filename.size() + 12) / 13;
		for (uint32_t i = entryCount, j = 0; i-- > 0; ++j)
		{
			LDIR& entry = *(LDIR*) &RawEntries.emplace_back();
			entry.Ord   = (uint8_t) (i + 1);
			if (j == 0)
				entry.Ord |= LDIR_ORD_LAST;
			entry.Attr      = ATTR_LDIR;
			entry.Type      = 0;
			entry.Chcksum   = shortNameChecksum;
			entry.FstClusLO = 0;
			memset(entry.Name1, 0xFF, 10);
			memset(entry.Name2, 0xFF, 12);
			memset(entry.Name3, 0xFF, 4);
			size_t charsLeft = std::min<size_t>(13, utf16filename.size() - i * 13);
			for (size_t k = 0; k < charsLeft; ++k)
			{
				if (k < 5)
					entry.Name1[k] = utf16filename[i * 13 + k];
				else if (k < 11)
					entry.Name2[k - 5] = utf16filename[i * 13 + k];
				else
					entry.Name3[k - 11] = utf16filename[i * 13 + k];
			}
			if (charsLeft < 5)
				entry.Name1[charsLeft] = u'\0';
			else if (charsLeft < 11)
				entry.Name2[charsLeft - 5] = u'\0';
			else if (charsLeft < 13)
				entry.Name3[charsLeft - 11] = u'\0';

			if (State->Options->Verbose)
				std::cout << "ImgGen INFO: FAT Raw Entry '" << (RawEntries.size() - 1) << "' is LFN ordinal '" << std::hex << std::setw(2) << std::setfill('0') << (uint16_t) entry.Ord << std::dec << "'\n";
		}
	}

	void DirectoryState::ParseRawEntries()
	{
		if (Modified)
			FlushRawEntries();

		ParsedEntries.clear();
		char16_t utf16FilenameBuf[261];
		uint8_t  checksums[20];
		for (size_t i = 0; i < RawEntries.size(); ++i)
		{
			if (RawEntries[i].Name[0] == 0x00)
				continue; // Skip unused entries

			bool    invalid    = false;
			uint8_t count      = 0;
			size_t  firstEntry = i;
			for (; count < 20 && (RawEntries[i].Attr & ATTR_LDIR) == ATTR_LDIR; ++i, ++count)
			{
				LDIR& ldir = *(LDIR*) &RawEntries[i];
				if (State->Options->Verbose)
					std::cout << "ImgGen INFO: parsing long filename entry '" << i << "' with ordinal '" << std::hex << std::setw(2) << std::setfill('0') << (uint16_t) ldir.Ord << std::dec << "'\n";
				if (count > 0 && ldir.Ord & LDIR_ORD_LAST)
				{
					std::cerr << "ImgGen ERROR: FAT Long Filename entry '" << firstEntry << "' ended with another Long Filename '" << i << "' in cluster '" << Cluster << "'\n";
					--i;
					invalid = true;
					break;
				}

				size_t index = ((ldir.Ord & LDIR_ORD_MASK) - 1) * 13;
				memcpy(utf16FilenameBuf + index, ldir.Name1, 10);
				memcpy(utf16FilenameBuf + index + 5, ldir.Name2, 12);
				memcpy(utf16FilenameBuf + index + 11, ldir.Name3, 4);
				if (ldir.Ord & LDIR_ORD_LAST)
					utf16FilenameBuf[index + 13] = u'\0';
				checksums[(ldir.Ord & LDIR_ORD_MASK) - 1] = ldir.Chcksum;
			}
			if (invalid)
				continue;

			while ((RawEntries[i].Attr & ATTR_LDIR) == ATTR_LDIR)
			{
				if (!invalid)
					std::cerr << "ImgGen ERROR: FAT Directory entry '" << i << "' in cluster '" << Cluster << "' has too many LDIR entries, skipping to next entry\n";
				invalid = true;
				if (((LDIR*) &RawEntries[i])->Ord & LDIR_ORD_LAST)
					break;
				++i;
			}
			if (invalid)
			{
				--i;
				continue;
			}

			DIR&  entry          = RawEntries[i];
			auto& parsedEntry    = ParsedEntries.emplace_back();
			parsedEntry.Cluster  = (entry.FstClusHI << 16) | entry.FstClusLO;
			parsedEntry.FileSize = entry.FileSize;
			memcpy(parsedEntry.ShortName, entry.Name, 11);
			parsedEntry.CreateTimeTenth = entry.CrtTimeTenth;
			parsedEntry.CreateTime      = entry.CrtTime;
			parsedEntry.CreateDate      = entry.CrtDate;
			parsedEntry.WriteTime       = entry.WrtTime;
			parsedEntry.WriteDate       = entry.WrtDate;
			parsedEntry.LastAccessDate  = entry.LstAccDate;
			parsedEntry.Attribute       = entry.Attr;
			if (State->Options->Verbose)
				std::cout << "ImgGen INFO: parsed entry '" << i << "' as '" << std::string_view { parsedEntry.ShortName, 11 } << "' with attribute '" << std::hex << std::setw(2) << std::setfill('0') << (uint16_t) parsedEntry.Attribute << std::dec << "'\n";
			if (count > 0)
			{
				uint8_t checksum = ShortNameChecksum(parsedEntry.ShortName);
				for (size_t j = 0; j < count; ++j)
				{
					if (checksums[j] != checksum)
					{
						std::cerr << "ImgGen ERROR: FAT Long Filename entry '" << j << "' in directory entry '" << i << "' in cluster '" << Cluster << "' has incorrect checksum '" << checksums[j] << "' (expected '" << checksum << "'), will fall back to short name\n";
						invalid = true;
					}
				}
				if (invalid)
					FillFilename(parsedEntry.Filename, parsedEntry.ShortName, entry.NTRes & NTLN_NAME, entry.NTRes & NTLN_EXT);
				else
					parsedEntry.Filename = UTF::UTF16ToUTF32(utf16FilenameBuf);
			}
			else
			{
				FillFilename(parsedEntry.Filename, parsedEntry.ShortName, entry.NTRes & NTLN_NAME, entry.NTRes & NTLN_EXT);
			}
			if (State->Options->Verbose)
				std::cout << "ImgGen INFO: entry was given filename '" << UTF::UTF32ToUTF8(parsedEntry.Filename) << "'\n";
		}
	}

	void DirectoryState::FlushRawEntries()
	{
		if (!Modified)
			return;
		RawEntries.clear();
		for (auto& entry : ParsedEntries)
		{
			bool lowercaseFilename  = false;
			bool lowercaseExtension = false;
			if (entry.Attribute == ATTR_VOLUME_ID)
			{
				memset(entry.ShortName, ' ', 11);
				auto utf8Filename = UTF::UTF32ToUTF8(entry.Filename);
				for (size_t i = 0; i < std::min<size_t>(11, utf8Filename.size()); ++i)
					entry.ShortName[i] = utf8Filename[i];
			}
			else
			{
				bool needsLongName = needsLongName = FillShortName(entry.Filename, entry.ShortName, lowercaseFilename, lowercaseExtension);
				if (needsLongName)
				{
					GenShortNameTail(entry.ShortName);
					uint8_t checksum = ShortNameChecksum(entry.ShortName);
					AddLDIREntries(entry.Filename, checksum);
				}
			}

			DIR& rawEntry = RawEntries.emplace_back();
			memcpy(rawEntry.Name, entry.ShortName, 11);
			rawEntry.Attr         = entry.Attribute;
			rawEntry.NTRes        = (lowercaseFilename ? NTLN_NAME : 0) | (lowercaseExtension ? NTLN_EXT : 0);
			rawEntry.CrtTimeTenth = entry.CreateTimeTenth;
			rawEntry.CrtTime      = entry.CreateTime;
			rawEntry.CrtDate      = entry.CreateDate;
			rawEntry.LstAccDate   = entry.LastAccessDate;
			rawEntry.FstClusHI    = entry.Cluster >> 16;
			rawEntry.WrtTime      = entry.WriteTime;
			rawEntry.WrtDate      = entry.WriteDate;
			rawEntry.FstClusLO    = (uint16_t) entry.Cluster;
			rawEntry.FileSize     = entry.FileSize;

			if (State->Options->Verbose)
				std::cout << "ImgGen INFO: FAT Raw Entry '" << (RawEntries.size() - 1) << "' with name '" << std::string_view { (const char*) rawEntry.Name, 11 } << "' has attribute '" << std::hex << std::setw(2) << std::setfill('0') << (uint16_t) rawEntry.Attr << ", " << std::setw(2) << std::setfill('0') << (uint16_t) entry.Attribute << std::dec << "', cluster '" << (rawEntry.FstClusHI << 16 | rawEntry.FstClusLO) << "'\n";
		}
		if (Cluster == 0)
		{
			State->FlushRootDirectory(*this);
		}
		else
		{
			size_t bytesPerCluster   = 512 * State->ClusterSize;
			size_t entriesPerCluster = bytesPerCluster / sizeof(DIR);
			RawEntries.resize((RawEntries.size() + entriesPerCluster) / entriesPerCluster * entriesPerCluster);
			State->EnsureClusters(Cluster, (RawEntries.size() * sizeof(DIR) + bytesPerCluster - 1) / bytesPerCluster);
			State->WriteClusters(RawEntries.data(), (RawEntries.size() * sizeof(DIR) + bytesPerCluster - 1) / bytesPerCluster, Cluster);
		}
		Modified = false;
	}

	void DirectoryState::MarkModified()
	{
		Modified = true;
	}

	DirectoryEntry* DirectoryState::AddEntry(std::u32string_view filename)
	{
		filename = filename.substr(0, 260);

		uint32_t estRawEntryConsumption = 1;
		if (Cluster == 0)
		{
			char shortNameBuf[11];
			bool lowercaseName      = false;
			bool lowercaseExtension = false;
			bool needsLongName      = FillShortName(filename, shortNameBuf, lowercaseName, lowercaseExtension);
			if (needsLongName)
				estRawEntryConsumption += (UTF::UTF32ToUTF16Length(filename) + 12) / 13;
			if (EstRawEntryCount + estRawEntryConsumption >= State->MaxRootEntryCount)
				return nullptr;
		}
		if (GetEntry(filename))
			return nullptr;

		uint16_t date, time;
		uint8_t  timeTenth;
		FATNow(date, time, timeTenth);
		auto& entry           = ParsedEntries.emplace_back();
		entry.Cluster         = 0;
		entry.FileSize        = 0;
		entry.Filename        = filename;
		entry.CreateTimeTenth = timeTenth;
		entry.CreateTime      = time;
		entry.CreateDate      = date;
		entry.WriteTime       = time;
		entry.WriteDate       = date;
		entry.LastAccessDate  = date;
		entry.Attribute       = 0x00;
		MarkModified();
		if (Cluster == 0)
			EstRawEntryCount += estRawEntryConsumption;
		if (State->Options->Verbose)
			std::cout << "ImgGen INFO: FAT directory entry '" << UTF::UTF32ToUTF8(entry.Filename) << "' added to directory '" << UTF::UTF32ToUTF8(Path) << "'\n";
		return &entry;
	}

	DirectoryEntry* DirectoryState::GetEntry(std::u32string_view filename)
	{
		filename = filename.substr(0, 260);
		for (auto& entry : ParsedEntries)
		{
			if (CompareFilenames(entry.Filename, filename))
				return &entry;
		}
		return nullptr;
	}

	DirectoryEntry* DirectoryState::RenameEntry(std::u32string_view oldFilename, std::u32string_view newFilename)
	{
		int32_t deltaRawEntryCount = 0;
		if (Cluster == 0)
		{
			char shortNameBuf[11];
			bool lowercaseName      = false;
			bool lowercaseExtension = false;
			bool needsLongName      = FillShortName(newFilename, shortNameBuf, lowercaseName, lowercaseExtension);
			if (needsLongName)
				deltaRawEntryCount += (UTF::UTF32ToUTF16Length(newFilename) + 12) / 13;

			needsLongName = FillShortName(oldFilename, shortNameBuf, lowercaseName, lowercaseExtension);
			if (needsLongName)
				deltaRawEntryCount -= (UTF::UTF32ToUTF16Length(oldFilename) + 12) / 13;

			if (EstRawEntryCount + deltaRawEntryCount >= State->MaxRootEntryCount)
				return nullptr;
		}

		oldFilename = oldFilename.substr(0, 260);
		newFilename = newFilename.substr(0, 260);
		if (GetEntry(newFilename))
			return nullptr;

		auto entry = GetEntry(oldFilename);
		if (!entry)
			return nullptr;
		entry->Filename = newFilename;
		MarkModified();
		if (Cluster == 0)
			EstRawEntryCount += deltaRawEntryCount;
		if (State->Options->Verbose)
			std::cout << "ImgGen INFO: FAT Directory entry '" << UTF::UTF32ToUTF8(oldFilename) << "' renamed to '" << UTF::UTF32ToUTF8(newFilename) << "'\n";
		return entry;
	}

	static void FreeDirectoryRecursive(DirectoryState& directory, DirectoryEntry& entry)
	{
		DirectoryState* subDirectory = directory.State->CacheDirectory(directory, entry);
		if (subDirectory)
		{
			for (auto& subEntry : subDirectory->ParsedEntries)
			{
				if (subEntry.Filename == U"." || subEntry.Filename == U"..")
					continue;

				if (subEntry.Attribute & ATTR_DIRECTORY)
					FreeDirectoryRecursive(*subDirectory, subEntry);
				else
					directory.State->FreeClusters(subEntry.Cluster);
			}
		}
		directory.State->UncacheDirectory(directory, entry);
		directory.State->FreeClusters(entry.Cluster);
	}

	void DirectoryState::RemoveEntry(std::u32string_view filename)
	{
		filename = filename.substr(0, 260);
		if (filename == U"." || filename == U"..")
			return;
		auto itr = std::find_if(ParsedEntries.begin(), ParsedEntries.end(), [filename](const DirectoryEntry& entry) -> bool { return CompareFilenames(entry.Filename, filename); });
		if (itr == ParsedEntries.end())
			return;
		if (itr->Attribute & ATTR_DIRECTORY)
			FreeDirectoryRecursive(*this, *itr);
		else
			State->FreeClusters(itr->Cluster);
		if (Cluster == 0)
		{
			char shortNameBuf[11];
			bool lowercaseName      = false;
			bool lowercaseExtension = false;
			bool needsLongName      = FillShortName(filename, shortNameBuf, lowercaseName, lowercaseExtension);
			if (needsLongName)
				EstRawEntryCount -= (UTF::UTF32ToUTF16Length(filename) + 12) / 13;
			--EstRawEntryCount;
		}
		ParsedEntries.erase(itr);
		MarkModified();
		if (State->Options->Verbose)
			std::cout << "ImgGen INFO: FAT Directory entry '" << UTF::UTF32ToUTF8(filename) << "' removed\n";
	}

	DirectoryEntry* DirectoryState::NewDirectory(std::u32string_view dirname)
	{
		dirname    = dirname.substr(0, 260);
		auto entry = AddEntry(dirname);
		if (!entry)
			return nullptr;

		entry->Cluster   = State->AllocCluster();
		entry->FileSize  = 0;
		entry->Attribute = ATTR_DIRECTORY;
		State->FillClusters(0, entry->Cluster);
		auto subDirectory            = State->CacheDirectory(*this, *entry);
		auto dotEntry                = subDirectory->AddEntry(U".");
		dotEntry->Cluster            = entry->Cluster;
		dotEntry->CreateTimeTenth    = entry->CreateTimeTenth;
		dotEntry->CreateTime         = entry->CreateTime;
		dotEntry->CreateDate         = entry->CreateDate;
		dotEntry->WriteTime          = entry->WriteTime;
		dotEntry->WriteDate          = entry->WriteDate;
		dotEntry->LastAccessDate     = entry->LastAccessDate;
		dotEntry->Attribute          = ATTR_DIRECTORY;
		auto dotDotEntry             = subDirectory->AddEntry(U"..");
		dotDotEntry->Cluster         = Parent == 0 ? 0 : Cluster;
		dotDotEntry->CreateTimeTenth = CreateTimeTenth;
		dotDotEntry->CreateTime      = CreateTime;
		dotDotEntry->CreateDate      = CreateDate;
		dotDotEntry->WriteTime       = WriteTime;
		dotDotEntry->WriteDate       = WriteDate;
		dotDotEntry->LastAccessDate  = LastAccessDate;
		dotDotEntry->Attribute       = ATTR_DIRECTORY;
		if (State->Options->Verbose)
			std::cout << "ImgGen INFO: ^^ made into directory '" << entry->Cluster << "' '.' entry has attribute '" << std::hex << std::setw(2) << std::setfill('0') << (uint16_t) dotEntry->Attribute << "', '..' has attribute '" << std::setw(2) << std::setfill('0') << (uint16_t) dotDotEntry->Attribute << std::dec << "'\n";
		return entry;
	}

	DirectoryEntry* DirectoryState::NewFile(std::u32string_view filename)
	{
		filename = filename.substr(0, 260);
		return AddEntry(filename);
	}

	void DirectoryState::SetLabel(std::string_view label)
	{
		if (Parent != 0)
			return;

		label      = label.substr(0, 11);
		bool found = false;
		for (auto& entry : ParsedEntries)
		{
			if (entry.Attribute != ATTR_VOLUME_ID)
				continue;

			found          = true;
			entry.Filename = UTF::UTF8ToUTF32(label);
			if (State->Options->Verbose)
				std::cout << "ImgGen INFO: FAT root directory label renamed to '" << label << "'\n";
			break;
		}
		if (!found)
		{
			auto entry       = AddEntry(UTF::UTF8ToUTF32(label));
			entry->Attribute = ATTR_VOLUME_ID;
			if (State->Options->Verbose)
				std::cout << "ImgGen INFO: ^^ made into a root directory label\n";
		}
		MarkModified();
	}

	void State::FlushRootDirectory(DirectoryState& directory)
	{
		if (Type == EType::FAT32)
		{
			directory.FlushRawEntries();
			return;
		}
		uint32_t rootDirSector = FirstFATSector + FATCount * FATSize;
		for (size_t i = 0; i < directory.RawEntries.size() * sizeof(DIR) / 512; ++i)
			PartHelpers::WriteSector(*Partition, *FStream, (const uint8_t*) directory.RawEntries.data() + i * 512, rootDirSector + i);
	}

	void State::FlushCache()
	{
		for (auto& [path, dir] : CachedDirectories)
			dir.FlushRawEntries();
		CachedDirectories.clear();
	}

	DirectoryState* State::CacheDirectory(DirectoryState& parentDirectory, DirectoryEntry& entry)
	{
		std::u32string directoryPath = parentDirectory.Path + entry.Filename + U'/';
		auto           itr           = CachedDirectories.find(directoryPath);
		if (itr == CachedDirectories.end())
		{
			itr                       = CachedDirectories.insert(std::pair { directoryPath, DirectoryState {} }).first;
			auto& directory           = itr->second;
			directory.State           = this;
			directory.Parent          = parentDirectory.Cluster;
			directory.Cluster         = entry.Cluster;
			directory.Modified        = false;
			directory.Path            = std::move(directoryPath);
			directory.CreateTimeTenth = entry.CreateTimeTenth;
			directory.CreateTime      = entry.CreateTime;
			directory.CreateDate      = entry.CreateDate;
			directory.WriteTime       = entry.WriteTime;
			directory.WriteDate       = entry.WriteDate;
			directory.LastAccessDate  = entry.LastAccessDate;
			size_t   offset           = 0;
			uint32_t curCluster       = directory.Cluster;
			while (curCluster != 0)
			{
				directory.RawEntries.resize(directory.RawEntries.size() + 512 * ClusterSize / sizeof(DIR));
				curCluster = ReadCluster(directory.RawEntries.data() + offset, curCluster);
				offset    += 512 * ClusterSize / sizeof(DIR);
			}
			directory.ParseRawEntries();
		}
		return &itr->second;
	}

	void State::UncacheDirectory(DirectoryState& parentDirectory, DirectoryEntry& entry)
	{
		std::u32string directoryPath = parentDirectory.Path + entry.Filename + U'/';
		auto           itr           = CachedDirectories.find(directoryPath);
		if (itr == CachedDirectories.end())
			return;

		itr->second.FlushRawEntries();
		CachedDirectories.erase(itr);
	}

	DirectoryState* State::GetDirectory(std::u32string_view path)
	{
		if (path.empty() || path[0] != U'/')
			return nullptr;
		if (path == U"/")
			return &RootDir;
		if (path[path.size() - 1] != U'/')
			return GetDirectory(std::u32string { path } + U'/');

		if (Options->Verbose)
			std::cout << "ImgGen INFO: attempting to find directory '" << UTF::UTF32ToUTF8(path) << "' in cache\n";

		auto itr = CachedDirectories.find(path);
		return itr != CachedDirectories.end() ? &itr->second : nullptr;
	}

	DirectoryState* State::LoadDirectory(std::u32string_view path, bool create)
	{
		if (path.empty() || path[0] != U'/')
			return nullptr;
		if (path[path.size() - 1] != U'/')
			return LoadDirectory(std::u32string { path } + U'/');
		auto directory = GetDirectory(path);
		if (directory)
			return directory;

		if (Options->Verbose)
			std::cout << "ImgGen INFO: failed to find in cache, will attempt to load from the last cached directory\n";

		size_t knownEnd = std::min<size_t>(path.size(), path.find_last_of(U'/', path.size() - 2));
		while (knownEnd < path.size() && !directory)
		{
			if (Options->Verbose)
				std::cout << "ImgGen INFO: looking for parent directory '" << UTF::UTF32ToUTF8(path.substr(0, knownEnd + 1)) << "'\n";
			directory = GetDirectory(path.substr(0, knownEnd + 1));
			if (directory)
				break;
			knownEnd = std::min<size_t>(path.size(), path.find_last_of(U'/', knownEnd - 1));
		}
		if (knownEnd >= path.size() || !directory)
		{
			if (Options->Verbose)
				std::cout << "ImgGen INFO: failed to find any parent directories in cache, path must've been invalid\n";
			return nullptr;
		}

		if (Options->Verbose)
			std::cout << "ImgGen INFO: found parent directory '" << UTF::UTF32ToUTF8(directory->Path) << "' with path end of '" << knownEnd << "'\n";

		size_t directoryEnd = std::min<size_t>(path.size(), path.find_first_of(U'/', knownEnd + 1));
		while (directoryEnd < path.size())
		{
			if (Options->Verbose)
				std::cout << "ImgGen INFO: trying to get entry '" << UTF::UTF32ToUTF8(path.substr(knownEnd + 1, directoryEnd - knownEnd - 1)) << "' in directory '" << UTF::UTF32ToUTF8(directory->Path) << "'\n";
			auto subEntry = directory->GetEntry(path.substr(knownEnd + 1, directoryEnd - knownEnd - 1));
			if (!subEntry && create)
				subEntry = directory->NewDirectory(path.substr(knownEnd + 1, directoryEnd - knownEnd - 1));

			if (!subEntry)
			{
				directory = nullptr;
				break;
			}

			directory    = CacheDirectory(*directory, *subEntry);
			knownEnd     = directoryEnd;
			directoryEnd = std::min<size_t>(path.size(), path.find_first_of(U'/', knownEnd + 1));
		}
		return directory;
	}

	void State::SetFAT(uint32_t cluster, uint32_t nextCluster)
	{
		if (cluster >= MaxClusters)
			return;

		switch (Type)
		{
		case EType::FAT12:
		{
			uint8_t   el = cluster & 7;
			uint16_t* pV = (uint16_t*) ((uint8_t*) FAT[cluster >> 3].Values + el * 12 / 8);
			*pV          = (*pV & ~(0xFFF << (4 * (el & 1)))) | ((nextCluster & 0xFFF) << (4 * (el & 1)));
			break;
		}
		case EType::FAT16:
		{
			auto&   entry         = FAT[cluster / 6];
			uint8_t el            = cluster % 6;
			entry.Values[el >> 1] = (entry.Values[el >> 1] & ~(0xFFFF << (16 * (el & 1)))) | ((nextCluster & 0xFFFF) << (16 * (el & 1)));
			break;
		}
		case EType::FAT32:
		{
			auto& entry =
				FAT[cluster / 3];
			uint8_t el       = cluster % 3;
			entry.Values[el] = (entry.Values[el] & 0xF000'0000) | (nextCluster & 0x0FFF'FFFF);
			break;
		}
		}
	}

	void State::SetFATEnd(uint32_t cluster)
	{
		switch (Type)
		{
		case EType::FAT12: SetFAT(cluster, 0xFFF); break;
		case EType::FAT16: SetFAT(cluster, 0xFFFF); break;
		case EType::FAT32: SetFAT(cluster, 0x0FFF'FFFF); break;
		}
	}

	uint32_t State::GetFAT(uint32_t cluster)
	{
		if (cluster >= MaxClusters)
			return 0;

		switch (Type)
		{
		case EType::FAT12:
		{
			uint8_t el = cluster & 7;
			return (*(uint16_t*) ((uint8_t*) FAT[cluster >> 3].Values + el * 12 / 8) >> (4 * (el & 1))) & 0xFFF;
		}
		case EType::FAT16:
		{
			uint8_t el = cluster % 6;
			return (FAT[cluster / 6].Values[el >> 1] >> (16 * (el & 1))) & 0xFFFF;
		}
		case EType::FAT32: return FAT[cluster / 3].Values[cluster % 3] & 0x0FFF'FFFF;
		default: return 0;
		}
	}

	bool State::IsFATEnd(uint32_t cluster)
	{
		uint32_t nextCluster = GetFAT(cluster);
		switch (Type)
		{
		case EType::FAT12: return nextCluster >= 0xFF8;
		case EType::FAT16: return nextCluster >= 0xFFF8;
		case EType::FAT32: return nextCluster >= 0x0FFF'FFF8;
		default: return false;
		}
	}

	uint32_t State::AllocCluster()
	{
		uint32_t curCluster = NextFreeCluster;
		do
		{
			uint32_t value = GetFAT(curCluster);
			if (!value)
			{
				--FreeClusterCount;
				NextFreeCluster = (curCluster + 1) % MaxClusters;
				SetFATEnd(curCluster);
				if (Options->Verbose)
					std::cout << "ImgGen INFO: FAT allocated cluster '" << curCluster << "'\n";
				return curCluster;
			}
			curCluster = (curCluster + 1) % MaxClusters;
		}
		while (curCluster != NextFreeCluster);
		return 0;
	}

	uint32_t State::AllocClusters(uint32_t count)
	{
		if (count == 0)
			return 0;
		uint32_t firstCluster = AllocCluster();
		if (!firstCluster)
			return 0;
		return EnsureClusters(firstCluster, count);
	}

	uint32_t State::EnsureClusters(uint32_t firstCluster, uint32_t count)
	{
		for (uint32_t i = 1; i < count; ++i)
			firstCluster = NextCluster(firstCluster, true);
		return firstCluster;
	}

	void State::FreeCluster(uint32_t cluster)
	{
		SetFAT(cluster, 0);
		++FreeClusterCount;
		if (Options->Verbose)
			std::cout << "ImgGen INFO: FAT freed cluster '" << cluster << "'\n";
	}

	void State::FreeClusters(uint32_t firstCluster)
	{
		while (firstCluster && !IsFATEnd(firstCluster))
		{
			uint32_t nextCluster = GetFAT(firstCluster);
			FreeCluster(firstCluster);
			firstCluster = nextCluster;
		}
	}

	uint32_t State::NextCluster(uint32_t cluster, bool alloc)
	{
		if (!cluster || cluster >= MaxClusters)
			return 0U;
		if (IsFATEnd(cluster))
		{
			if (alloc)
			{
				uint32_t nextCluster = AllocCluster();
				SetFAT(cluster, nextCluster);
				SetFATEnd(nextCluster);
				return nextCluster;
			}
			return 0;
		}
		return GetFAT(cluster);
	}

	uint32_t State::NthCluster(uint32_t firstCluster, uint32_t count, bool alloc)
	{
		for (uint32_t i = 1; i < count && firstCluster; ++i)
			firstCluster = NextCluster(firstCluster, alloc);
		return firstCluster;
	}

	uint32_t State::WriteCluster(const void* data, uint32_t cluster, bool alloc)
	{
		if (cluster < 2)
			return 0;

		if (Options->Verbose)
			std::cout << "ImgGen INFO: FAT writing cluster '" << cluster << "'\n";
		for (size_t i = 0; i < ClusterSize; ++i)
			PartHelpers::WriteSector(*Partition, *FStream, (const uint8_t*) data + i * 512, FirstClusterSector + (cluster - 2) * ClusterSize + i);
		return NextCluster(cluster, alloc);
	}

	uint32_t State::ReadCluster(void* data, uint32_t cluster, bool alloc)
	{
		if (cluster < 2)
			return 0;

		if (Options->Verbose)
			std::cout << "ImgGen INFO: FAT reading cluster '" << cluster << "'\n";
		for (size_t i = 0; i < ClusterSize; ++i)
			PartHelpers::ReadSector(*Partition, *FStream, (uint8_t*) data + i * 512, FirstClusterSector + (cluster - 2) * ClusterSize + i);
		return NextCluster(cluster, alloc);
	}

	uint32_t State::WriteClusters(const void* data, uint32_t count, uint32_t firstCluster, bool extend)
	{
		uint32_t writeCount = 0;
		while (firstCluster && count)
		{
			firstCluster = WriteCluster((const uint8_t*) data + writeCount * ClusterSize * 512, firstCluster, extend && count > 1);
			--count;
			++writeCount;
		}
		return writeCount;
	}

	uint32_t State::ReadClusters(void* data, uint32_t count, uint32_t firstCluster, bool extend)
	{
		uint32_t readCount = 0;
		while (firstCluster && count)
		{
			firstCluster = ReadCluster((uint8_t*) data + readCount * ClusterSize * 512, firstCluster, extend && count > 1);
			--count;
			++readCount;
		}
		return readCount;
	}

	void State::FillClusters(uint8_t value, uint32_t firstCluster)
	{
		uint8_t* buffer = new uint8_t[512 * ClusterSize];
		memset(buffer, 0, 512 * ClusterSize);
		while (firstCluster)
			firstCluster = WriteCluster(buffer, firstCluster);
		delete[] buffer;
	}

	EFileType FileTypeFromAttribute(uint8_t attribute)
	{
		if (attribute == ATTR_VOLUME_ID)
			return EFileType::Reserved;
		if (attribute == ATTR_DIRECTORY ||
			attribute == ATTR_ARCHIVE)
			return EFileType::Directory;
		return EFileType::File;
	}

	uint64_t MaxFileSize()
	{
		return 0xFFFF'FFFF;
	}

	uint64_t GetMinPartitionSize()
	{
		return 8ULL * 8192;
	}

	uint64_t GetMaxPartitionSize()
	{
		return 8ULL * 0x0FFF'FFF7;
	}

	std::u32string NormalizePath(std::u32string_view path)
	{
		std::u32string result;
		result.reserve(path.size());
		result.push_back(U'/');
		size_t offset = 1;
		while (offset < path.size())
		{
			size_t segmentEnd = std::min<size_t>(path.size(), path.find_first_of(U'/', offset));
			if (segmentEnd - offset == 0 ||
				path.substr(offset, segmentEnd - offset) == U"/./")
			{
			}
			else if (path.substr(offset, segmentEnd - offset) == U"/../")
			{
				size_t parentEnd = result.size() < 2 ? ~0ULL : result.find_last_of(U'/', result.size() - 2);
				if (parentEnd >= result.size())
				{
					std::cerr << "ImgGen ERROR: path '" << UTF::UTF32ToUTF8(path) << "' attempting to go outside root directory\n";
					return {};
				}
				result.erase(result.begin() + parentEnd + 1, result.end());
			}
			else
			{
				result.append(path.substr(offset, 1 + segmentEnd - offset));
			}
			offset = std::min<size_t>(path.size(), path.find_first_not_of(U'/', segmentEnd + 1));
		}
		return result;
	}

	State* LoadState(ImgGenOptions& options, PartitionOptions& partition, std::fstream& fstream)
	{
		State* state     = new State();
		state->Options   = &options;
		state->Partition = &partition;
		state->FStream   = &fstream;

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

			uint32_t rootDirSectors   = (bpb.RootEntCnt * sizeof(DIR) + 511) / 512;
			state->ClusterSize        = bpb.SecPerClus;
			state->FATSize            = bpb.FATSz16 != 0 ? bpb.FATSz16 : bpb32.FATSz32; // INFO(MarcasRealAccount): This is some rather bad code, why did microsoft think this was ok???
			state->FATCount           = bpb.NumFATs;
			state->FirstFATSector     = bpb.RsvdSecCnt;
			state->FirstClusterSector = state->FirstFATSector + state->FATCount * state->FATSize + rootDirSectors;
			state->MaxClusters        = ((bpb.TotSec16 != 0 ? bpb.TotSec16 : bpb.TotSec32) - state->FirstClusterSector) / state->ClusterSize;

			if (state->MaxClusters < 4085)
				state->Type = EType::FAT12;
			else if (state->MaxClusters < 65525)
				state->Type = EType::FAT16;
			else
				state->Type = EType::FAT32;

			auto& rootDir            = state->RootDir;
			rootDir.State            = state;
			rootDir.Parent           = 0;
			rootDir.Modified         = false;
			rootDir.EstRawEntryCount = 0;
			rootDir.Path             = U"/";
			rootDir.CreateTimeTenth  = 0;
			rootDir.CreateTime       = 0;
			rootDir.CreateDate       = 0;
			rootDir.WriteTime        = 0;
			rootDir.WriteDate        = 0;
			rootDir.LastAccessDate   = 0;
			bool countFreeClusters   = true;
			if (state->Type == EType::FAT32)
			{
				PartHelpers::ReadSector(partition, fstream, &fsInfo, bpb32.FSInfo);
				bs             = static_cast<BS*>(&ebpb32);
				bootCode       = ebpb32.BootCode;
				bootCodeLength = sizeof(ebpb32.BootCode);

				if (bpb32.ExtFlags & BPB32_NON_MIRROR)
					state->CurFAT = bpb32.ExtFlags & BPB32_CUR_FAT;
				else
					state->CurFAT = 0x80;

				if (fsInfo.LeadSig == 0x41615252 &&
					fsInfo.StrucSig == 0x61417272 &&
					fsInfo.TrailSig == 0xAA550000)
				{
					state->FreeClusterCount = fsInfo.FreeCount;
					state->NextFreeCluster  = fsInfo.NxtFree;
					countFreeClusters       = false;
				}

				state->CurFATSector = state->FirstFATSector + (state->CurFAT & 0x7F) * state->FATSize;

				if (options.Verbose)
					std::cout << "ImgGen INFO: Allocating '" << state->FATSize << "' sectors per FAT\n";
				state->FAT.resize((state->FATSize * 512 + sizeof(FATEntry) - 1) / sizeof(FATEntry));
				for (size_t i = 0; i < state->FATSize; ++i)
					PartHelpers::ReadSector(partition, fstream, (uint8_t*) state->FAT.data() + i * 512, state->CurFATSector + i);

				state->MaxRootEntryCount = ~0U;
				rootDir.Cluster          = bpb32.RootClus;
				size_t   offset          = 0;
				uint32_t curCluster      = rootDir.Cluster;
				while (curCluster != 0)
				{
					rootDir.RawEntries.resize(rootDir.RawEntries.size() + 512 * state->ClusterSize / sizeof(DIR));
					curCluster = state->ReadCluster(rootDir.RawEntries.data() + offset, curCluster);
					offset    += 512 * state->ClusterSize / sizeof(DIR);
				}
			}
			else
			{
				bs             = static_cast<BS*>(&ebpb16);
				bootCode       = ebpb16.BootCode;
				bootCodeLength = sizeof(ebpb16.BootCode);

				state->CurFAT       = 0x80;
				state->CurFATSector = state->FirstFATSector + (state->CurFAT & 0x7F) * state->FATSize;

				if (options.Verbose)
					std::cout << "ImgGen INFO: Allocating '" << state->FATSize << "' sectors per FAT\n";
				state->FAT.resize((state->FATSize * 512 + sizeof(FATEntry) - 1) / sizeof(FATEntry));
				for (size_t i = 0; i < state->FATSize; ++i)
					PartHelpers::ReadSector(partition, fstream, (uint8_t*) state->FAT.data() + i * 512, state->CurFATSector + i);

				state->MaxRootEntryCount = bpb.RootEntCnt;
				rootDir.Cluster          = 0;
				rootDir.RawEntries.resize(bpb.RootEntCnt);
				for (size_t i = 0; i < rootDirSectors; ++i)
					PartHelpers::ReadSector(partition, fstream, (uint8_t*) rootDir.RawEntries.data() + i * 512, state->FirstFATSector + state->FATCount * state->FATSize + i);
			}
			rootDir.ParseRawEntries();

			if (countFreeClusters)
			{
				state->FreeClusterCount = 0;
				state->NextFreeCluster  = ~0U;
				for (size_t i = 0; i < state->MaxClusters; ++i)
				{
					if (!state->GetFAT((uint32_t) i))
					{
						if (state->NextFreeCluster == ~0U)
							state->NextFreeCluster = (uint32_t) i;
						++state->FreeClusterCount;
					}
				}

				if (state->Type == EType::FAT32)
				{
					fsInfo.LeadSig   = 0x41615252;
					fsInfo.StrucSig  = 0x61417272;
					fsInfo.FreeCount = state->FreeClusterCount;
					fsInfo.NxtFree   = state->NextFreeCluster;
					fsInfo.TrailSig  = 0xAA550000;
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
			bpb.BytsPerSec       = 512;
			bpb.SecPerClus       = 8;
			bpb.NumFATs          = 2;
			bpb.Media            = 0xF8;
			bpb.SecPerTrk        = 0x3F;
			bpb.NumHeads         = 0xFF;
			bpb.HiddSec          = (uint32_t) std::min<uint64_t>(partition.ActualStart, 0xFFFF'FFFF);
			ebpb32.SignatureWord = 0xAA55;

			uint32_t sectorCount = (uint32_t) (partition.ActualEnd - partition.ActualStart);
			if (sectorCount < 8192)
				state->Type = EType::FAT12;
			else if (sectorCount < 1048576)
				state->Type = EType::FAT16;
			else
				state->Type = EType::FAT32;

			if (state->Type == EType::FAT32)
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
				memcpy(bs->FilSysType, state->Type == EType::FAT16 ? "FAT16   " : "FAT12   ", 8);
			}

			memcpy(bootCode, c_LegacyBootProtection, std::min<size_t>(bootCodeLength, 512)); // min not needed, but just to be safe

			uint16_t date, time;
			uint8_t  timeTenth;
			FATNow(date, time, timeTenth);
			bs->DrvNum  = 0x80;
			bs->BootSig = 0x29;
			bs->VolID   = date << 16 | time;
			memcpy(bs->VolLab, partition.Name.c_str(), std::min<size_t>(11, partition.Name.size()));

			bpb.JmpBoot[1]          = bootCode - bpbBuf;
			state->ClusterSize      = 8;
			state->FATCount         = 2;
			uint32_t rootDirSectors = (bpb.RootEntCnt * 32 + 511) / 512;
			uint32_t tempVal        = 256 * state->ClusterSize + state->FATCount;
			if (state->Type == EType::FAT32)
				tempVal >>= 1;
			state->FATSize            = (sectorCount - bpb.RsvdSecCnt - rootDirSectors + tempVal - 1) / tempVal;
			state->FirstFATSector     = bpb.RsvdSecCnt;
			state->CurFAT             = 0x80;
			state->CurFATSector       = state->FirstFATSector;
			state->FirstClusterSector = state->FirstFATSector + state->FATCount * state->FATSize + rootDirSectors;
			state->MaxClusters        = (sectorCount - state->FirstClusterSector) / state->ClusterSize;
			state->FreeClusterCount   = state->MaxClusters;
			state->NextFreeCluster    = 2;

			if (options.Verbose)
				std::cout << "ImgGen INFO: Allocating '" << state->FATSize << "' sectors per FAT\n";
			state->FAT.resize((state->FATSize * 512 + sizeof(FATEntry) - 1) / sizeof(FATEntry));
			switch (state->Type)
			{
			case EType::FAT12: state->SetFAT(0, 0xFF8); break;
			case EType::FAT16: state->SetFAT(0, 0xFFF8); break;
			case EType::FAT32: state->SetFAT(0, 0x0FFF'FFF8); break;
			}
			state->SetFATEnd(1);

			auto& rootDir            = state->RootDir;
			rootDir.State            = state;
			rootDir.Parent           = 0;
			rootDir.Modified         = false;
			rootDir.EstRawEntryCount = 0;
			rootDir.Path             = U"/";
			rootDir.CreateTimeTenth  = 0;
			rootDir.CreateTime       = 0;
			rootDir.CreateDate       = 0;
			rootDir.WriteTime        = 0;
			rootDir.WriteDate        = 0;
			rootDir.LastAccessDate   = 0;
			if (state->Type == EType::FAT32)
			{
				state->MaxRootEntryCount = ~0U;
				rootDir.Cluster          = state->AllocCluster();
				state->FillClusters(0, rootDir.Cluster);

				bpb.FATSz16     = 0;
				bpb32.FATSz32   = state->FATSize;
				bpb32.ExtFlags  = 0x0000;
				bpb32.FSVer     = 0x0000;
				bpb32.RootClus  = rootDir.Cluster;
				bpb32.FSInfo    = 1;
				bpb32.BkBootSec = 6;

				fsInfo.LeadSig   = 0x41615252;
				fsInfo.StrucSig  = 0x61417272;
				fsInfo.FreeCount = state->FreeClusterCount;
				fsInfo.NxtFree   = state->NextFreeCluster;
				fsInfo.TrailSig  = 0xAA550000;
			}
			else
			{
				state->MaxRootEntryCount = bpb.RootEntCnt;
				rootDir.Cluster          = 0;
				uint8_t zeroes[512];
				memset(zeroes, 0, 512);
				uint32_t rootDirSector = state->FirstFATSector + state->FATCount * state->FATSize;
				for (size_t i = 0; i < rootDirSectors; ++i)
					PartHelpers::WriteSector(partition, fstream, zeroes, rootDirSector + i);

				bpb.FATSz16 = (uint16_t) state->FATSize;
			}

			rootDir.SetLabel(partition.Name);

			PartHelpers::WriteSector(partition, fstream, bpbBuf, 0);

			if (state->Type == EType::FAT32)
			{
				PartHelpers::WriteSector(partition, fstream, &fsInfo, bpb32.FSInfo);
				PartHelpers::WriteSector(partition, fstream, bpbBuf, bpb32.BkBootSec);
				PartHelpers::WriteSector(partition, fstream, &fsInfo, bpb32.BkBootSec + bpb32.FSInfo);
			}
		}
		return state;
	}

	void SaveState(State* state)
	{
		if (!state)
			return;

		state->FlushCache();

		uint8_t bpbBuf[512];
		FSInfo  fsInfo;
		memset(bpbBuf, 0, 512);
		memset(&fsInfo, 0, 512);
		PartHelpers::ReadSector(*state->Partition, *state->FStream, bpbBuf, 0);

		EBPB16&  ebpb16         = *(EBPB16*) bpbBuf;
		EBPB32&  ebpb32         = *(EBPB32*) bpbBuf;
		BPB&     bpb            = static_cast<BPB&>(ebpb16);
		BPB32&   bpb32          = static_cast<BPB32&>(ebpb32);
		BS*      bs             = nullptr;
		uint8_t* bootCode       = nullptr;
		size_t   bootCodeLength = 0;

		if (state->Type == EType::FAT32)
		{
			bs             = static_cast<BS*>(&ebpb32);
			bootCode       = ebpb32.BootCode;
			bootCodeLength = sizeof(ebpb32.BootCode);

			bpb32.ExtFlags = state->CurFAT & 0x80 ? 0x0000 : (BPB32_NON_MIRROR | state->CurFAT);

			PartHelpers::ReadSector(*state->Partition, *state->FStream, &fsInfo, bpb32.FSInfo);
			fsInfo.FreeCount = state->FreeClusterCount;
			fsInfo.NxtFree   = state->NextFreeCluster;
			PartHelpers::WriteSector(*state->Partition, *state->FStream, &fsInfo, bpb32.FSInfo);
			PartHelpers::WriteSector(*state->Partition, *state->FStream, &fsInfo, bpb32.BkBootSec + bpb32.FSInfo);
		}
		else
		{
			bs             = static_cast<BS*>(&ebpb16);
			bootCode       = ebpb16.BootCode;
			bootCodeLength = sizeof(ebpb16.BootCode);
		}

		memcpy(bs->VolLab, state->Partition->Name.c_str(), std::min<size_t>(11, state->Partition->Name.size()));
		state->RootDir.SetLabel(state->Partition->Name);
		state->FlushRootDirectory(state->RootDir);

		if (!state->Options->RetainBootCode)
		{
			memcpy(bpb.JmpBoot, "\xEB\x00\x90", 3);
			bpb.JmpBoot[1] = bootCode - bpbBuf;
			memcpy(bootCode, c_LegacyBootProtection, std::min<size_t>(bootCodeLength, 512)); // min not needed, but just to be safe
		}

		if (state->CurFAT & 0x80)
		{
			for (size_t i = 0; i < state->FATCount; ++i)
			{
				if (state->Options->Verbose)
					std::cout << "ImgGen INFO: writing FAT " << (i + 1) << " to sectors '" << (state->FirstFATSector + i * state->FATSize) << "' to '" << (state->FirstFATSector + (i + 1) * state->FATSize - 1) << "'\n";
				for (size_t j = 0; j < state->FATSize; ++j)
					PartHelpers::WriteSector(*state->Partition, *state->FStream, (const uint8_t*) state->FAT.data() + j * 512, state->FirstFATSector + i * state->FATSize + j);
			}
		}
		else
		{
			if (state->Options->Verbose)
				std::cout << "ImgGen INFO: writing FAT " << state->CurFAT << " to sectors '" << (state->FirstFATSector + state->CurFAT * state->FATSize) << "' to '" << (state->FirstFATSector + (state->CurFAT + 1) * state->FATSize - 1) << "'\n";
			for (size_t j = 0; j < state->FATSize; ++j)
				PartHelpers::WriteSector(*state->Partition, *state->FStream, (const uint8_t*) state->FAT.data() + j * 512, state->CurFATSector + j);
		}

		PartHelpers::WriteSector(*state->Partition, *state->FStream, bpbBuf, 0);
		if (state->Type == EType::FAT32)
			PartHelpers::WriteSector(*state->Partition, *state->FStream, bpbBuf, bpb32.BkBootSec);

		delete state;
	}

	bool Exists(State* state, std::u32string_view path)
	{
		if (!state || path.empty())
			return false;

		size_t fileBegin       = path.find_last_of(U'/');
		auto   parentDirectory = state->LoadDirectory(path.substr(0, fileBegin + 1));
		if (!parentDirectory)
			return false;
		if (fileBegin == path.size() - 1)
			return true;
		return parentDirectory->GetEntry(path.substr(fileBegin + 1)) != nullptr;
	}

	EFileType GetType(State* state, std::u32string_view path)
	{
		if (!state || path.empty())
			return EFileType::Invalid;

		size_t fileBegin       = path.find_last_of(U'/');
		auto   parentDirectory = state->LoadDirectory(path.substr(0, fileBegin + 1));
		if (!parentDirectory)
			return EFileType::Invalid;
		if (fileBegin == path.size() - 1)
			return EFileType::Directory;
		auto entry = parentDirectory->GetEntry(path.substr(fileBegin + 1));
		if (!entry)
			return EFileType::Invalid;
		return FileTypeFromAttribute(entry->Attribute);
	}

	void Delete(State* state, std::u32string_view path)
	{
		if (!state || path.empty())
			return;

		size_t fileBegin = path.find_last_of(U'/');
		if (fileBegin == path.size() - 1)
		{
			size_t parentEnd       = path.find_last_of(U'/', fileBegin - 1);
			auto   parentDirectory = state->LoadDirectory(path.substr(0, parentEnd + 1));
			parentDirectory->RemoveEntry(path.substr(parentEnd + 1, fileBegin - parentEnd - 1));
		}
		else
		{
			auto parentDirectory = state->LoadDirectory(path.substr(0, fileBegin + 1));
			parentDirectory->RemoveEntry(path.substr(fileBegin + 1));
		}
	}

	void CreateDirectory(State* state, std::u32string_view path)
	{
		if (!state || path.empty())
			return;

		if (path[path.size() - 1] == U'/')
			path = path.substr(0, path.size() - 1);

		size_t parentEnd       = path.find_last_of(U'/');
		auto   parentDirectory = state->LoadDirectory(path.substr(0, parentEnd + 1), true);
		if (parentDirectory)
			parentDirectory->NewDirectory(path.substr(parentEnd + 1));
	}

	FileState GetFile(State* state, std::u32string_view path, bool create)
	{
		if (!state || path.empty())
			return {};

		size_t parentEnd       = path.find_last_of(U'/');
		auto   parentDirectory = state->LoadDirectory(path.substr(0, parentEnd + 1), create);
		if (!parentDirectory)
			return {};
		auto entry = parentDirectory->GetEntry(path.substr(parentEnd + 1));
		if (!entry)
		{
			entry = parentDirectory->NewFile(path.substr(parentEnd + 1));
			if (!entry)
				return {};
		}
		else if (FileTypeFromAttribute(entry->Attribute) != EFileType::File)
		{
			return {};
		}

		FileState fileState;
		fileState.State        = state;
		fileState.Directory    = parentDirectory;
		fileState.Entry        = entry;
		fileState.ReadCluster  = fileState.Entry->Cluster;
		fileState.WriteCluster = fileState.Entry->Cluster;
		fileState.ReadOffset   = 0;
		fileState.WriteOffset  = 0;
		return fileState;
	}

	void CloseFile(FileState& fileState)
	{
		fileState.Directory->MarkModified();
	}

	void EnsureFileSize(FileState& fileState, uint32_t fileSize)
	{
		size_t bytesPerCluster = 512 * fileState.State->ClusterSize;
		if (!fileState.Entry->Cluster)
			fileState.Entry->Cluster = fileState.State->AllocCluster();
		fileState.State->EnsureClusters(fileState.Entry->Cluster, (fileSize + bytesPerCluster - 1) / bytesPerCluster);
	}

	uint32_t GetFileSize(FileState& fileState)
	{
		return fileState.Entry->FileSize;
	}

	void SetWriteTime(FileState& fileState, TimePoint time)
	{
		auto today                 = std::chrono::floor<std::chrono::days>(time);
		auto ymd                   = std::chrono::year_month_day(today);
		auto hms                   = std::chrono::hh_mm_ss(time - today);
		fileState.Entry->WriteDate = ToFATDate((unsigned) ymd.day(), (unsigned) ymd.month(), (int) ymd.year());
		fileState.Entry->WriteTime = ToFATTime(hms.hours().count(), hms.minutes().count(), hms.seconds().count());
	}

	void SetAccessTime(FileState& fileState, TimePoint time)
	{
		auto today                      = std::chrono::floor<std::chrono::days>(time);
		auto ymd                        = std::chrono::year_month_day(today);
		fileState.Entry->LastAccessDate = ToFATDate((unsigned) ymd.day(), (unsigned) ymd.month(), (int) ymd.year());
	}

	TimePoint GetCreateTime(FileState& fileState)
	{
		uint8_t  day, month;
		uint16_t year;
		uint8_t  hour, minute, second, tenths = fileState.Entry->CreateTimeTenth;
		FromFATDate(fileState.Entry->CreateDate, day, month, year);
		FromFATTime(fileState.Entry->CreateTime, hour, minute, second);
		auto ymd = std::chrono::year_month_day(std::chrono::year { year }, std::chrono::month { month }, std::chrono::day { day });
		return std::chrono::sys_days { ymd } + std::chrono::hours { hour } + std::chrono::minutes { minute } + std::chrono::seconds { second } + std::chrono::milliseconds { tenths * 10 };
	}

	TimePoint GetWriteTime(FileState& fileState)
	{
		uint8_t  day, month;
		uint16_t year;
		uint8_t  hour, minute, second;
		FromFATDate(fileState.Entry->WriteDate, day, month, year);
		FromFATTime(fileState.Entry->WriteTime, hour, minute, second);
		auto ymd = std::chrono::year_month_day(std::chrono::year { year }, std::chrono::month { month }, std::chrono::day { day });
		return std::chrono::sys_days { ymd } + std::chrono::hours { hour } + std::chrono::minutes { minute } + std::chrono::seconds { second };
	}

	TimePoint GetAccessTime(FileState& fileState)
	{
		uint8_t  day, month;
		uint16_t year;
		FromFATDate(fileState.Entry->LastAccessDate, day, month, year);
		auto ymd = std::chrono::year_month_day(std::chrono::year { year }, std::chrono::month { month }, std::chrono::day { day });
		return std::chrono::sys_days { ymd };
	}

	void SeekRead(FileState& fileState, uint32_t amount, int direction)
	{
		if (direction < 0)
			fileState.ReadOffset -= amount;
		else if (direction > 0)
			fileState.ReadOffset += amount;
		else
			fileState.ReadOffset = amount;
		fileState.ReadCluster = fileState.State->NthCluster(fileState.Entry->Cluster, fileState.ReadOffset / (512 * fileState.State->ClusterSize));
	}

	void SeekWrite(FileState& fileState, uint32_t amount, int direction)
	{
		if (direction < 0)
			fileState.WriteOffset -= amount;
		else if (direction > 0)
			fileState.WriteOffset += amount;
		else
			fileState.WriteOffset = amount;
		fileState.WriteCluster = fileState.State->NthCluster(fileState.Entry->Cluster, fileState.WriteOffset / (512 * fileState.State->ClusterSize));
	}

	uint32_t Read(FileState& fileState, void* buffer, uint32_t size)
	{
		if (!fileState.ReadCluster || fileState.ReadOffset >= fileState.Entry->FileSize)
			return 0;

		uint32_t bytesPerCluster = 512 * fileState.State->ClusterSize;

		uint32_t initialBytes = std::min<uint32_t>(512 - (fileState.ReadOffset % bytesPerCluster), size);
		uint32_t offset       = 0;
		uint8_t* temp         = nullptr;
		if (initialBytes > 0)
		{
			temp                 = new uint8_t[bytesPerCluster];
			uint32_t nextCluster = fileState.State->ReadCluster(temp, fileState.ReadCluster);
			memcpy(buffer, temp, initialBytes);
			offset               += initialBytes;
			fileState.ReadOffset += initialBytes;
			size                 -= initialBytes;
			if ((fileState.ReadOffset % bytesPerCluster) == 0)
				fileState.ReadCluster = nextCluster;
		}
		if (!size)
		{
			delete[] temp;
			return offset;
		}

		uint32_t clusterCount = size / bytesPerCluster;
		uint32_t remainder    = size % bytesPerCluster;
		for (size_t i = 0; i < clusterCount && fileState.ReadCluster; ++i)
		{
			fileState.ReadCluster = fileState.State->ReadCluster((uint8_t*) buffer + offset, fileState.ReadCluster);
			offset               += bytesPerCluster;
		}
		if (!fileState.ReadCluster)
		{
			delete[] temp;
			return offset;
		}

		if (remainder > 0)
		{
			if (!temp)
				temp = new uint8_t[bytesPerCluster];
			fileState.State->ReadCluster(temp, fileState.ReadCluster);
			memcpy((uint8_t*) buffer + offset, temp, remainder);
			offset               += remainder;
			fileState.ReadOffset += remainder;
		}
		delete[] temp;
		return offset;
	}

	uint32_t Write(FileState& fileState, const void* buffer, uint32_t size)
	{
		EnsureFileSize(fileState, fileState.WriteOffset + size);
		if (!fileState.WriteCluster)
			fileState.WriteCluster = fileState.State->NthCluster(fileState.Entry->Cluster, fileState.WriteOffset / (512 * fileState.State->ClusterSize));
		if (!fileState.WriteCluster)
			return 0;

		uint32_t bytesPerCluster = 512 * fileState.State->ClusterSize;

		uint32_t initialBytes = std::min<uint32_t>(512 - (fileState.WriteOffset % bytesPerCluster), size);
		uint32_t offset       = 0;
		uint8_t* temp         = nullptr;
		if (initialBytes > 0)
		{
			temp = new uint8_t[bytesPerCluster];
			memcpy(temp, buffer, initialBytes);
			uint32_t nextCluster   = fileState.State->WriteCluster(temp, fileState.WriteCluster, false);
			offset                += initialBytes;
			fileState.WriteOffset += initialBytes;
			size                  -= initialBytes;
			if (size > 0 && !nextCluster)
				nextCluster = fileState.State->NextCluster(fileState.WriteCluster, true);
			if ((fileState.WriteOffset % bytesPerCluster) == 0)
				fileState.WriteCluster = nextCluster;
		}
		if (!size)
		{
			if (fileState.WriteOffset > fileState.Entry->FileSize)
				fileState.Entry->FileSize = fileState.WriteOffset;
			delete[] temp;
			return offset;
		}

		uint32_t clusterCount = size / bytesPerCluster;
		uint32_t remainder    = size % bytesPerCluster;
		for (size_t i = 0; i < clusterCount && fileState.WriteCluster; ++i)
		{
			fileState.WriteCluster = fileState.State->WriteCluster((const uint8_t*) buffer + offset, fileState.WriteCluster, i < clusterCount - 1);
			offset                += bytesPerCluster;
		}
		if (!fileState.WriteCluster)
		{
			if (fileState.WriteOffset > fileState.Entry->FileSize)
				fileState.Entry->FileSize = fileState.WriteOffset;
			delete[] temp;
			return offset;
		}

		if (remainder > 0)
		{
			if (!temp)
				temp = new uint8_t[bytesPerCluster];
			memcpy(temp, (const uint8_t*) buffer + offset, remainder);
			fileState.State->WriteCluster(temp, fileState.WriteCluster, false);
			offset                += remainder;
			fileState.WriteOffset += remainder;
		}
		if (fileState.WriteOffset > fileState.Entry->FileSize)
			fileState.Entry->FileSize = fileState.WriteOffset;
		delete[] temp;
		return offset;
	}
} // namespace FAT