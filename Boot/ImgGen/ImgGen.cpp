#include "FAT32.h"
#include "GPT.h"
#include "GUID.h"
#include "UTF.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

enum class ESectorMode : uint8_t
{
	Relative,
	EndRelative,
	Absolute
};

enum class EPartitionFormat : uint8_t
{
	None,
	FAT32,
	exFAT
};

struct PartitionOptions
{
	ESectorMode StartMode = ESectorMode::Relative;
	ESectorMode EndMode   = ESectorMode::Relative;
	uint64_t    Start     = 0;
	uint64_t    End       = 0;

	GUID        TypeGUID = {};
	std::string Name;

	EPartitionFormat Format = EPartitionFormat::None;
};

struct FileCopy
{
	uint8_t     Partition;
	std::string From;
	std::string To;
};

struct Options
{
	bool Verbose       = false;
	bool HelpPresented = false;

	std::filesystem::path OutputFilepath      = "disk.img";
	uint64_t              ImageSize           = 0;
	uint64_t              PhysicalSectorSize  = 4096;
	uint64_t              TransferGranularity = 1048576;
	uint8_t               PartitionCount      = 0;

	std::vector<FileCopy> FileCopies;
	PartitionOptions      PartitionOptions[128];
};

static bool ParseOptions(Options& options, int argc, const char* const* argv);

using KVArgs = std::unordered_map<std::string, std::string>;

static uint64_t ParseByteCount(std::string_view arg);
static bool     ParseSectorPos(std::string_view arg, ESectorMode& mode, uint64_t& pos);
static KVArgs   ParseKVArgs(std::string_view arg);

static std::string Unstringify(std::string_view arg);
static int         CaselessStringCompare(std::string_view lhs, std::string_view rhs);

int main(int argc, char** argv)
{
	Options options;
	if (!ParseOptions(options, argc, argv))
	{
		std::cerr << "Failed to parse all options fully\n";
		return 1;
	}
	if (options.HelpPresented)
		return 0;

	if (options.ImageSize == 0)
	{
		std::cerr << "Image size is 0 bytes, skipping process\n";
		return 0;
	}

	std::filesystem::create_directories(options.OutputFilepath.parent_path());
	if (!std::filesystem::exists(options.OutputFilepath))
	{
		std::ofstream file(options.OutputFilepath, std::ios::binary);
		if (!file)
		{
			std::cerr << "Failed to create image file\n";
			return 1;
		}
		file.close();
		if (options.Verbose)
			std::cout << "Created image file\n";
	}

	std::fstream imageStream(options.OutputFilepath, std::ios::binary | std::ios::in | std::ios::out);
	if (!imageStream)
	{
		std::cerr << "Failed to open created image file\n";
		return 1;
	}

	options.ImageSize = (options.ImageSize + options.PhysicalSectorSize - 1) / options.PhysicalSectorSize * options.PhysicalSectorSize;

	{
		char buf[32768];
		memset(buf, 0, 32768);

		if (options.Verbose)
			std::cout << "Filling output file with 0s\n0/" << options.ImageSize;

		imageStream.seekp(0);
		for (size_t i = 0; i < options.ImageSize / 32768; ++i)
		{
			imageStream.write(buf, 32768);
			if (options.Verbose)
				std::cout << '\r' << (i * 32768) << '/' << options.ImageSize;
		}
		if (options.ImageSize % 32768 > 0)
			imageStream.write(buf, options.ImageSize % 32768);
		if (options.Verbose)
			std::cout << '\r' << options.ImageSize << '/' << options.ImageSize << '\n';
	}

	uint64_t partitionAlignment = std::max<uint64_t>(options.PhysicalSectorSize, options.TransferGranularity) / 512;

	GPT::GPTState gptState;
	gptState.Header.BackupLBA           = options.ImageSize / 512 - 1;
	gptState.Header.DiskGUID            = RandomGUID();
	gptState.Header.PartitionEntryCount = options.PartitionCount;
	GPT::InitState(gptState);
	uint64_t currentLBA = (gptState.Header.FirstUsableLBA + partitionAlignment - 1) / partitionAlignment * partitionAlignment;
	for (size_t i = 0; i < options.PartitionCount; ++i)
	{
		auto& partitionOption = options.PartitionOptions[i];

		auto& entry         = gptState.Partitions[i];
		entry.TypeGUID      = partitionOption.TypeGUID;
		entry.PartitionGUID = RandomGUID();
		switch (partitionOption.StartMode)
		{
		case ESectorMode::Relative:
			entry.FirstLBA = (currentLBA + partitionOption.Start + partitionAlignment - 1) / partitionAlignment * partitionAlignment;
			break;
		case ESectorMode::EndRelative:
			entry.FirstLBA = (gptState.Header.LastUsableLBA - partitionOption.End) / partitionAlignment * partitionAlignment;
			break;
		case ESectorMode::Absolute:
			entry.FirstLBA = (partitionOption.Start + partitionAlignment - 1) / partitionAlignment * partitionAlignment;
			break;
		}
		if (entry.FirstLBA > gptState.Header.LastUsableLBA)
		{
			std::cerr << "Attempting to create partition outside usable sectors\n";
			return 1;
		}
		currentLBA = entry.FirstLBA;
		switch (partitionOption.EndMode)
		{
		case ESectorMode::Relative:
			entry.LastLBA = (currentLBA + std::max<uint64_t>(1, partitionOption.End) + partitionAlignment - 1) / partitionAlignment * partitionAlignment;
			break;
		case ESectorMode::EndRelative:
			entry.LastLBA = (gptState.Header.LastUsableLBA - std::max<uint64_t>(1, partitionOption.End)) / partitionAlignment * partitionAlignment;
			break;
		case ESectorMode::Absolute:
			entry.LastLBA = (partitionOption.End + partitionAlignment - 1) / partitionAlignment * partitionAlignment;
			break;
		}
		if (entry.LastLBA > gptState.Header.LastUsableLBA)
		{
			std::cerr << "Attempting to create partition outside usable sectors\n";
			return 1;
		}
		if (entry.LastLBA < entry.FirstLBA)
		{
			std::cerr << "Attempting to create negative partition range\n";
			return 1;
		}
		currentLBA           = entry.LastLBA + 1;
		entry.AttributeFlags = 0;
		memset(entry.Name, 0, sizeof(entry.Name));
		auto utf16Name = UTF8ToUTF16(partitionOption.Name);
		for (size_t i = 0; i < std::min<size_t>(36, utf16Name.size()); ++i)
			entry.Name[i] = utf16Name[i];

		if (options.Verbose)
		{
			auto utf8Name = UTF16ToUTF8(std::u16string_view { (const char16_t*) entry.Name, std::min<size_t>(36, utf16Name.size()) });
			std::cout << "Created partition " << (i + 1) << " \"" << utf8Name << "\":\n";
			std::cout << "  Starts at sector " << entry.FirstLBA << ", ends at sector " << entry.LastLBA << '\n';
			std::cout << "  Type GUID '" << entry.TypeGUID << "'\n";
			std::cout << "  Partition GUID '" << entry.PartitionGUID << "'\n";
		}
	}

	GPT::WriteState(gptState, imageStream);

	for (size_t i = 0; i < options.PartitionCount; ++i)
	{
		auto& partitionOption = options.PartitionOptions[i];

		if (partitionOption.Format == EPartitionFormat::None)
			continue;

		if (options.Verbose)
		{
			std::cout << "Formatting partition " << (i + 1);
			switch (partitionOption.Format)
			{
			case EPartitionFormat::FAT32: std::cout << " as FAT32\n"; break;
			case EPartitionFormat::exFAT: std::cout << " as exFAT\n"; break;
			default: std::cout << " as UNKNOWN\n"; break;
			}
		}

		switch (partitionOption.Format)
		{
		case EPartitionFormat::FAT32:
		{
			FAT32::FSState fsState;
			fsState.FirstLBA    = gptState.Partitions[i].FirstLBA;
			fsState.LastLBA     = gptState.Partitions[i].LastLBA;
			fsState.Verbose     = options.Verbose;
			fsState.NoShortName = true;
			FAT32::InitState(fsState, partitionOption.Name);

			for (auto& copy : options.FileCopies)
			{
				if (copy.Partition != i)
					continue;

				for (auto itr : std::filesystem::recursive_directory_iterator { copy.From, std::filesystem::directory_options::skip_permission_denied | std::filesystem::directory_options::follow_directory_symlink })
				{
					if (!itr.is_regular_file())
						continue;

					std::ifstream file { itr.path(), std::ios::binary | std::ios::ate };
					if (!file)
						continue;

					std::vector<char> data(file.tellg());
					file.seekg(0);
					file.read(data.data(), data.size());
					file.close();

					std::string filepath = copy.To + std::filesystem::relative(itr.path(), copy.From).string();

					if (options.Verbose)
						std::cout << "Copying file '" << itr.path() << "' to '" << filepath << "' (" << data.size() << " bytes)\n";
					FAT32::WriteFile(fsState, imageStream, filepath, data.data(), (uint32_t) data.size());
					if (options.Verbose)
						std::cout << "Copied file  '" << itr.path() << "' to '" << filepath << "'\n";
				}
			}

			FAT32::WriteState(fsState, imageStream);
			break;
		}
		case EPartitionFormat::exFAT:
			std::cerr << "exFAT format not implemented\n";
			break;
		default:
			std::cerr << "Attempting to format partition " << (i + 1) << " as UNKNOWN format, skipping partition\n";
			break;
		}
	}
	return 0;
}

bool ParseOptions(Options& options, int argc, const char* const* argv)
{
	for (size_t i = 1; i < argc; ++i)
	{
		std::string_view arg = argv[i];
		if (arg == "-v")
		{
			options.Verbose = true;
		}
		else if (arg == "-h")
		{
			options.HelpPresented = true;

			std::cout << "ImgGen Help\n"
					  << "Options:\n"
					  << "  -h                   Presents this\n"
					  << "  -v                   Enables verbosity, gives most debug information\n"
					  << "  -o <path>            Output filepath\n"
					  << "  -s <num>[suffix]     Image size\n"
					  << "  -b <num>[suffix]     Physical sector size (default 4KiB)\n"
					  << "  -t <num>[suffix]     Transfer granularity (default 1MiB)\n"
					  << "  -p [<key=value>,...] Add partition\n"
					  << "    'start' and 'end':\n"
					  << "      Relative:     '~' or '+<num>[suffix]'\n"
					  << "      End relative: '^' or '-<num>[suffix]'\n"
					  << "      Absolute:     '<num>[suffix]'\n"
					  << "    'type' Partition type GUID or one of:\n"
					  << "      EF00 : EFI System Partition\n"
					  << "    'name=<str>' Name of partition max 36 characters\n"
					  << "  -f [<key=value>,...] Format partition\n"
					  << "    'p=<num>' Partition\n"
					  << "    'type'    Type to format partition as, allowed are:\n"
					  << "      FAT32 : File Allocation Table 32 bit (WIP)\n"
					  << "      exFAT : Extended File Allocation Table (WIP)\n"
					  << "  -c [<key=value>,...] Copy files to partition\n"
					  << "    'p=<num>'     Partition\n"
					  << "    'from=<path>' Path to copy from\n"
					  << "    'to=<path>'   Path to copy to\n";
			return true;
		}
	}

	for (size_t i = 1; i < argc; ++i)
	{
		std::string_view arg = argv[i];
		if (arg == "-v" || arg == "-h")
		{
		}
		else if (arg == "-o")
		{
			options.OutputFilepath = argv[++i];
			if (options.Verbose)
				std::cout << "Arg -o '" << options.OutputFilepath << "'\n";
		}
		else if (arg == "-s")
		{
			options.ImageSize = ParseByteCount(argv[++i]);
			if (options.Verbose)
				std::cout << "Arg -s '" << argv[i] << "': " << options.ImageSize << " bytes\n";
		}
		else if (arg == "-b")
		{
			options.PhysicalSectorSize = ParseByteCount(argv[++i]);
			if (options.Verbose)
				std::cout << "Arg -b '" << argv[i] << "': " << options.PhysicalSectorSize << " bytes\n";
		}
		else if (arg == "-t")
		{
			options.TransferGranularity = ParseByteCount(argv[++i]);
			if (options.Verbose)
				std::cout << "Arg -t '" << argv[i] << "': " << options.TransferGranularity << " bytes\n";
		}
		else if (arg == "-p")
		{
			if (options.PartitionCount >= 128)
			{
				std::cerr << "Attempting to create too many partitions, max 128 supported\n";
				return false;
			}

			auto kvs = ParseKVArgs(argv[++i]);
			if (options.Verbose)
			{
				std::cout << "Arg -p '" << argv[i] << "':\n";
				// for (auto& [key, value] : kvs)
				// 	std::cout << "  '" << key << "' = '" << value << "'\n";
			}

			auto& partOptions = options.PartitionOptions[options.PartitionCount];
			if (kvs.contains("start"))
			{
				if (!ParseSectorPos(kvs["start"], partOptions.StartMode, partOptions.Start))
				{
					std::cerr << "  Failed to parse partition start for partition " << options.PartitionCount << '\n';
					return false;
				}
			}
			else
			{
				partOptions.StartMode = ESectorMode::Relative;
				partOptions.Start     = 0;
			}
			if (kvs.contains("end"))
			{
				if (!ParseSectorPos(kvs["end"], partOptions.EndMode, partOptions.End))
				{
					std::cerr << "  Failed to parse partition end for partition " << options.PartitionCount << '\n';
					return false;
				}
			}
			else
			{
				partOptions.EndMode = ESectorMode::Relative;
				partOptions.End     = 1;
			}
			if (kvs.contains("type"))
				partOptions.TypeGUID = GPT::ParsePartitionType(kvs["type"]);
			if (kvs.contains("name"))
				partOptions.Name = kvs["name"];

			if (options.Verbose)
			{
				switch (partOptions.StartMode)
				{
				case ESectorMode::Relative:
					if (partOptions.Start == 0)
						std::cout << "  Starts after current sector\n";
					else
						std::cout << "  Starts " << partOptions.Start << " bytes after current sector\n";
					break;
				case ESectorMode::EndRelative:
					if (partOptions.Start <= 1)
						std::cout << "  Starts at last sector, but why would you want that???\n";
					else
						std::cout << "  Starts " << partOptions.Start << " bytes before last sector\n";
					break;
				case ESectorMode::Absolute:
					std::cout << "  Starts at byte " << partOptions.Start << '\n';
					break;
				}

				switch (partOptions.EndMode)
				{
				case ESectorMode::Relative:
					if (partOptions.End <= 1)
						std::cout << "  Ends after start\n";
					else
						std::cout << "  Ends " << partOptions.End << " bytes after start\n";
					break;
				case ESectorMode::EndRelative:
					if (partOptions.End <= 1)
						std::cout << "  Ends at last sector\n";
					else
						std::cout << "  Ends " << partOptions.End << " bytes before last sector\n";
					break;
				case ESectorMode::Absolute:
					std::cout << "  Ends at byte " << partOptions.End << '\n';
					break;
				}

				std::cout << "  Type '" << partOptions.TypeGUID << "'\n";
				std::cout << "  Name '" << partOptions.Name << "'\n";
			}
			++options.PartitionCount;
		}
		else if (arg == "-f")
		{
			auto kvs = ParseKVArgs(argv[++i]);
			if (options.Verbose)
			{
				std::cout << "Arg -f '" << argv[i] << "':\n";
				// for (auto& [key, value] : kvs)
				// 	std::cout << "  '" << key << "' = '" << value << "'\n";
			}

			if (!kvs.contains("p"))
			{
				std::cerr << "Attempting to format unknown partition, add 'p=<num>' to the key value pairs\n";
				return false;
			}
			if (!kvs.contains("type"))
			{
				std::cerr << "Attempting to format partition with unknown type, add 'type=<type>' to the key value pairs\n";
				return false;
			}

			uint64_t partitionNumber = std::strtoull(kvs["p"].c_str(), nullptr, 10);
			if (partitionNumber == 0 || partitionNumber > options.PartitionCount)
			{
				std::cerr << "Attempting to format partition " << partitionNumber << " which doesn't exist\n";
				return false;
			}
			--partitionNumber;

			auto& partOptions = options.PartitionOptions[partitionNumber];
			auto& type        = kvs["type"];
			if (CaselessStringCompare(type, "fat32") == 0)
			{
				partOptions.Format = EPartitionFormat::FAT32;
			}
			else if (CaselessStringCompare(type, "exfat") == 0)
			{
				partOptions.Format = EPartitionFormat::exFAT;
			}
			else
			{
				std::cerr << "Attempting to format partition with unknown type '" << type << "'\n";
				return false;
			}

			if (options.Verbose)
			{
				std::cout << "  Partition " << (partitionNumber + 1) << '\n';
				switch (partOptions.Format)
				{
				case EPartitionFormat::None: std::cout << "  Type 'None'\n"; break;
				case EPartitionFormat::FAT32: std::cout << "  Type 'FAT32'\n"; break;
				case EPartitionFormat::exFAT: std::cout << "  Type 'exFAT'\n"; break;
				default: std::cout << "  Type 'UNKNOWN'\n"; break;
				}
			}
		}
		else if (arg == "-c")
		{
			auto kvs = ParseKVArgs(argv[++i]);
			if (options.Verbose)
			{
				std::cout << "Arg -c '" << argv[i] << "':\n";
				// for (auto& [key, value] : kvs)
				// 	std::cout << "  '" << key << "' = '" << value << "'\n";
			}

			if (!kvs.contains("p"))
			{
				std::cerr << "Attempting to copy to unknown partition, add 'p=<num>' to the key value pairs\n";
				return false;
			}
			if (!kvs.contains("from"))
			{
				std::cerr << "Attempting to copy from unknown filepath, add 'from=<path>' to the key value pairs\n";
				return false;
			}
			if (!kvs.contains("to"))
			{
				std::cerr << "Attempting to copy to unknown filepath, add 'to=<path>' to the key value pairs\n";
				return false;
			}

			uint64_t partitionNumber = std::strtoull(kvs["p"].c_str(), nullptr, 10);
			if (partitionNumber == 0 || partitionNumber > options.PartitionCount)
			{
				std::cerr << "Attempting to copy to partition " << partitionNumber << " which doesn't exist\n";
				return false;
			}
			--partitionNumber;

			auto& copy     = options.FileCopies.emplace_back();
			copy.Partition = (uint8_t) partitionNumber;
			copy.From      = kvs["from"];
			copy.To        = kvs["to"];

			if (options.Verbose)
			{
				std::cout << "  Partition " << (partitionNumber + 1) << '\n';
				std::cout << "  From '" << copy.From << "'\n";
				std::cout << "  To '" << copy.To << "'\n";
			}
		}
		else
		{
			std::cerr << "Unknown argument '" << arg << "'\n";
			return false;
		}
	}

	return true;
}

uint64_t ParseByteCount(std::string_view arg)
{
	uint64_t value  = 0;
	size_t   offset = 0;
	while (offset < arg.size())
	{
		char c = arg[offset];
		switch (c)
		{
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			value *= 10;
			value += c - '0';
			break;
		default:
			goto BREAKOUT;
		}
		++offset;
		continue;
	BREAKOUT:
		break;
	}

	size_t           suffixStart = arg.find_first_not_of(' ', offset);
	size_t           suffixEnd   = arg.find_first_of(' ', suffixStart);
	std::string_view suffix      = arg.substr(suffixStart, suffixEnd - suffixStart);
	if (CaselessStringCompare(suffix, "kib") == 0)
		value <<= 10;
	else if (CaselessStringCompare(suffix, "mib") == 0)
		value <<= 20;
	else if (CaselessStringCompare(suffix, "gib") == 0)
		value <<= 30;
	else if (CaselessStringCompare(suffix, "tib") == 0)
		value <<= 40;
	else if (CaselessStringCompare(suffix, "pib") == 0)
		value <<= 50;
	else if (CaselessStringCompare(suffix, "eib") == 0)
		value <<= 60;
	else if (CaselessStringCompare(suffix, "kb") == 0)
		value *= 1'000;
	else if (CaselessStringCompare(suffix, "mb") == 0)
		value *= 1'000'000;
	else if (CaselessStringCompare(suffix, "gb") == 0)
		value *= 1'000'000'000;
	else if (CaselessStringCompare(suffix, "tb") == 0)
		value *= 1'000'000'000'000ULL;
	else if (CaselessStringCompare(suffix, "pb") == 0)
		value *= 1'000'000'000'000'000ULL;
	else if (CaselessStringCompare(suffix, "eb") == 0)
		value *= 1'000'000'000'000'000'000ULL;
	return value;
}

bool ParseSectorPos(std::string_view arg, ESectorMode& mode, uint64_t& pos)
{
	if (arg == "~")
	{
		mode = ESectorMode::Relative;
		pos  = 0;
		return true;
	}
	else if (arg == "^")
	{
		mode = ESectorMode::EndRelative;
		pos  = 0;
		return true;
	}

	if (arg.empty())
	{
		mode = ESectorMode::Relative;
		pos  = 0;
		return true;
	}

	if (arg[0] == '+')
	{
		mode = ESectorMode::Relative;
		pos  = (ParseByteCount(arg.substr(1)) + 511) / 512;
		return true;
	}
	else if (arg[0] == '-')
	{
		mode = ESectorMode::EndRelative;
		pos  = (ParseByteCount(arg.substr(1)) + 511) / 512;
		return true;
	}
	mode = ESectorMode::Absolute;
	pos  = (ParseByteCount(arg) + 511) / 512;
	return true;
}

KVArgs ParseKVArgs(std::string_view arg)
{
	KVArgs kvs {};

	size_t offset = 0;
	while (offset < arg.size())
	{
		size_t keyEnd = offset;
		{
			bool inString = false;
			bool escaped  = false;
			while (keyEnd < arg.size())
			{
				char c = arg[keyEnd];
				switch (c)
				{
				case '\\':
					if (inString)
						escaped = !escaped;
					break;
				case '"':
					if (!escaped)
						inString = !inString;
					escaped = false;
					break;
				case '=':
				case ',':
					if (escaped)
					{
						escaped = false;
						break;
					}
					if (inString)
						break;
					goto BREAKOUT1;
				default:
					escaped = false;
					break;
				}
				++keyEnd;
				continue;
			BREAKOUT1:
				break;
			}
		}

		if (arg[keyEnd] == ',')
		{
			kvs[Unstringify(arg.substr(offset, keyEnd - offset))] = "";

			offset = keyEnd + 1;
			continue;
		}

		size_t valueEnd = keyEnd + 1;
		{
			bool inString = false;
			bool escaped  = false;
			while (valueEnd < arg.size())
			{
				char c = arg[valueEnd];
				switch (c)
				{
				case '\\':
					if (inString)
						escaped = !escaped;
					break;
				case '"':
					if (!escaped)
						inString = !inString;
					escaped = false;
					break;
				case ',':
					if (escaped)
					{
						escaped = false;
						break;
					}
					if (inString)
						break;
					goto BREAKOUT2;
				default:
					escaped = false;
					break;
				}
				++valueEnd;
				continue;
			BREAKOUT2:
				break;
			}
		}

		kvs[Unstringify(arg.substr(offset, keyEnd - offset))] = Unstringify(arg.substr(keyEnd + 1, valueEnd - keyEnd - 1));

		offset = valueEnd + 1;
	}

	return kvs;
}

std::string Unstringify(std::string_view arg)
{
	size_t start = arg.find_first_not_of(' ');
	size_t end   = arg.find_last_not_of(' ');
	arg          = arg.substr(start, end - start + 1);
	if (arg.size() < 2 || arg[0] != '"' || arg[arg.size() - 1] != '"')
		return std::string { arg };

	std::string str { arg.substr(1, arg.size() - 2) };
	for (size_t i = 0; i < str.size(); ++i)
	{
		if (str[i] != '\\')
			continue;
		switch (str[++i])
		{
		case '\\':
		case '"':
		case '\'':
			str.erase(--i);
			break;
		case 'n':
			str.replace(--i, 2, "\n");
			break;
		default:
			str.erase(--i);
			break;
		}
	}
	return str;
}

int CaselessStringCompare(std::string_view lhs, std::string_view rhs)
{
	if (lhs.size() > rhs.size())
		return -1;
	if (lhs.size() < rhs.size())
		return 1;

	for (size_t i = 0; i < lhs.size(); ++i)
	{
		char a = std::tolower(lhs[i]);
		char b = std::tolower(rhs[i]);
		if (a > b)
			return -1;
		if (a < b)
			return 1;
	}
	return 0;
}