#include "Args/Args.hpp"
#include "Args/KV.hpp"
#include "PartitionSchemes/GPT.hpp"
#include "PartitionSchemes/MBR.hpp"
#include "PartitionTypes/FAT.hpp"
#include "PartitionTypes/Helpers.hpp"
#include "State.hpp"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

static bool             ParseBoolean(std::string_view arg, bool def = false);
static int64_t          ParseInt(std::string_view arg);
static int64_t          ParseByteCount(std::string_view arg);
static uint64_t         ParseSectorOffset(std::string_view arg, ESectorMode& mode);
static EPartitionFormat ParsePartitionFormat(std::string_view arg);
static EPartitionScheme ParsePartitionScheme(std::string_view arg);

int main(int argc, char** argv)
{
	ImgGenOptions options;

	KV::State outputKVState;
	KV::State partitionKVState;
	KV::State formatKVState;
	KV::State copyKVState;
	outputKVState.RegisterOption({ .Name = "path", .Required = true }, { .Syntax = "<path>", .Description = "Image filepath" });
	outputKVState.RegisterOption({ .Name = "size" }, { .Syntax = "<num>[suffix]", .Description = "Image size" });
	outputKVState.RegisterOption({ .Name = "type" }, { .Syntax = "<partition scheme>", .Description = "Partition scheme to use (default GPT)" });
	outputKVState.RegisterOption({ .Name = "keep-bootcode" }, { .Syntax = "<boolean>", .Description = "Should we try and keep the bootcode?" });
	outputKVState.RegisterOption({ .Name = "physical-size" }, { .Syntax = "<num>[suffix]", .Description = "Image physical size (default 4KiB)" });
	outputKVState.RegisterOption({ .Name = "transfer-granularity" }, { .Syntax = "<num>[suffix]", .Description = "Image transfer granularity (default 1MiB)" });
	partitionKVState.RegisterOption({ .Name = "start", .Required = true }, { .Syntax = "Relative     : ',' or '+<num>[suffix]'\nEnd Relative : '.' or '-<num>[suffix]'\nAbsolute     : '<num>[suffix]'", .Description = "Start of partition" });
	partitionKVState.RegisterOption({ .Name = "end", .Required = true }, { .Syntax = "Relative     : ',' or '+<num>[suffix]'\nEnd Relative : '.' or '-<num>[suffix]'\nAbsolute     : '<num>[suffix]'", .Description = "End of partition" });
	partitionKVState.RegisterOption({ .Name = "type", .Required = true }, { .Syntax = "<GUID/ID>", .Description = "Partition type GUID or short ID" });
	partitionKVState.RegisterOption({ .Name = "name", .Required = true }, { .Syntax = "<string>", .Description = "Name of partition (Max of 36 characters)" });
	formatKVState.RegisterOption({ .Name = "partition", .Required = true }, { .Syntax = "<number>", .Description = "Partition number" });
	formatKVState.RegisterOption({ .Name = "type", .Required = true }, { .Syntax = "<filesystem>", .Description = "Partition format" });
	copyKVState.RegisterOption({ .Name = "partition", .Required = true }, { .Syntax = "<number>", .Description = "Partition number" });
	copyKVState.RegisterOption({ .Name = "from", .Required = true }, { .Syntax = "<path>", .Description = "Host filepath to copy from" });
	copyKVState.RegisterOption({ .Name = "to", .Required = true }, { .Syntax = "<path>", .Description = "Filepath to copy to" });

	Args::RegisterOption({ .Name = "verbose" }, { .Syntax = "", .Description = "Enable verbose output" });
	Args::RegisterOption({ .Name = "output", .MinArgs = 1, .MaxArgs = ~0ULL, .MinCount = 1 }, { .Syntax = "<key=value> ...", .Description = "Output file", .HelpFn = &KV::HelpFn, .HelpUserdata = &outputKVState });
	Args::RegisterOption({ .Name = "partition", .MinArgs = 1, .MaxArgs = ~0ULL, .MaxCount = 128 }, { .Syntax = "<key=value> ...", .Description = "Add partition", .HelpFn = &KV::HelpFn, .HelpUserdata = &partitionKVState });
	Args::RegisterOption({ .Name = "format", .MinArgs = 1, .MaxArgs = ~0ULL, .MaxCount = ~0ULL }, { .Syntax = "<key=value> ...", .Description = "Format partition", .HelpFn = &KV::HelpFn, .HelpUserdata = &formatKVState });
	Args::RegisterOption({ .Name = "copy", .MinArgs = 1, .MaxArgs = ~0ULL, .MaxCount = ~0ULL }, { .Syntax = "<key=value> ...", .Description = "Copy files to partition", .HelpFn = &KV::HelpFn, .HelpUserdata = &copyKVState });
	{
		auto argOptions = Args::Handle(argc, argv);
		if (argOptions.ShouldExit)
			return argOptions.ExitCode;
		if (!argOptions["output"])
		{
			std::cerr << "ImgGen ERROR: output not specified\n";
			return 1;
		}

		options.Verbose = argOptions["verbose"];
		{
			auto& outputArgs = argOptions["output"];
			auto  kvOptions  = outputKVState.HandleArgs(outputArgs.Options[0]);
			if (kvOptions.ShouldExit)
				return kvOptions.ExitCode;

			options.CanExpand           = kvOptions["size"];
			options.RetainBootCode      = ParseBoolean(kvOptions["keep-bootcode"]);
			options.OutputFilepath      = std::filesystem::absolute(kvOptions["path"].Value);
			options.ImageSize           = kvOptions["size"] ? ParseByteCount(kvOptions["size"]) : 0;
			options.PartitionScheme     = kvOptions["type"] ? ParsePartitionScheme(kvOptions["type"]) : EPartitionScheme::GPT;
			options.PhysicalSize        = kvOptions["physical-size"] ? ParseByteCount(kvOptions["physical-size"]) : 4096;
			options.TransferGranularity = kvOptions["transfer-granularity"] ? ParseByteCount(kvOptions["transfer-granularity"]) : 1 << 20;
		}
		for (auto& partitionOptions : argOptions["partition"].Options)
		{
			auto kvOptions = partitionKVState.HandleArgs(partitionOptions);
			if (kvOptions.ShouldExit)
				return kvOptions.ExitCode;

			auto& partitionOption = options.PartitionOptions[options.PartitionCount++];
			partitionOption.Start = ParseSectorOffset(kvOptions["start"], partitionOption.StartMode);
			partitionOption.End   = ParseSectorOffset(kvOptions["end"], partitionOption.EndMode);
			switch (options.PartitionScheme)
			{
			case EPartitionScheme::MBR: partitionOption.Type.iden = MBR::ParsePartitionType(kvOptions["type"]); break;
			case EPartitionScheme::GPT: partitionOption.Type.guid = GPT::ParsePartitionType(kvOptions["type"]); break;
			}
			partitionOption.Name = kvOptions["name"];
		}
		for (auto& formatOptions : argOptions["format"].Options)
		{
			auto kvOptions = formatKVState.HandleArgs(formatOptions);
			if (kvOptions.ShouldExit)
				return kvOptions.ExitCode;

			int64_t partitionNumber = ParseInt(kvOptions["partition"]);
			if (partitionNumber < 1 || partitionNumber > options.PartitionCount)
			{
				std::cerr << "ImgGen ERROR: '--format' option was given a partition number outside the valid range 1 <= p <= " << options.PartitionOptions << '\n';
				return 1;
			}

			auto& partitionOption  = options.PartitionOptions[partitionNumber - 1];
			partitionOption.Format = ParsePartitionFormat(kvOptions["type"]);
		}
		for (auto& copyOptions : argOptions["copy"].Options)
		{
			auto kvOptions = copyKVState.HandleArgs(copyOptions);
			if (kvOptions.ShouldExit)
				return kvOptions.ExitCode;

			int64_t partitionNumber = ParseInt(kvOptions["partition"]);
			if (partitionNumber < 1 || partitionNumber > options.PartitionCount)
			{
				std::cerr << "ImgGen ERROR: '--copy' option was given a partition number outside the valid range 1 <= p <= " << options.PartitionOptions << '\n';
				return 1;
			}
			options.PartitionOptions[partitionNumber - 1].Copies.emplace_back(FileCopy { .From = std::filesystem::absolute(kvOptions["from"].Value), .To = kvOptions["to"].Value });
		}

		if (options.PartitionCount == 0)
		{
			std::cerr << "ImgGen ERROR: need at least one partition definition\n";
			return 1;
		}
	}

	PartHelpers::ValidateOptions(options);
	PartHelpers::PartitionRanges(options);

	std::filesystem::create_directories(options.OutputFilepath.parent_path());
	if (!std::filesystem::exists(options.OutputFilepath))
	{
		std::ofstream file(options.OutputFilepath, std::ios::binary);
		if (!file)
		{
			std::cerr << "ImgGen ERROR: failed to create output file " << options.OutputFilepath << '\n';
			return 1;
		}
		file.close();
		if (options.Verbose)
			std::cout << "ImgGen INFO: Created output file " << options.OutputFilepath << '\n';
	}

	std::fstream outputFile(options.OutputFilepath, std::ios::binary | std::ios::in | std::ios::out | std::ios::ate);
	if (!outputFile)
	{
		std::cerr << "ImgGen ERROR: failed to open output file " << options.OutputFilepath << '\n';
		return 1;
	}
	{
		size_t curSize = outputFile.tellg();
		if (curSize < options.ImageSize)
		{
			uint8_t* zeros = new uint8_t[0x1'0000];
			memset(zeros, 0, 0x1'0000);
			if (options.Verbose)
				std::cout << "ImgGen INFO: Filling output file with 0s\n"
						  << curSize << '/' << options.ImageSize;
			size_t toWrite = options.ImageSize - curSize;
			for (size_t i = 0; i < toWrite / 0x1'0000; ++i)
			{
				outputFile.write((const char*) zeros, 0x1'0000);
				if (options.Verbose)
					std::cout << '\r' << (curSize + (i * 0x1'0000)) << '/' << options.ImageSize;
			}
			if (toWrite % 0x1'0000 > 0)
				outputFile.write((const char*) zeros, toWrite % 0x1'0000);
			if (options.Verbose)
				std::cout << '\r' << options.ImageSize << '/' << options.ImageSize << '\n';
			delete[] zeros;
		}
		outputFile.seekg(0);
		outputFile.seekp(0);
	}

	switch (options.PartitionScheme)
	{
	case EPartitionScheme::MBR:
		MBR::SetupPartitions(options, outputFile);
		break;
	case EPartitionScheme::GPT:
		GPT::SetupPartitions(options, outputFile);
		break;
	}

	uint8_t* transferBuffer = new uint8_t[65536];
	memset(transferBuffer, 0, 65536);
	for (size_t i = 0; i < options.PartitionCount; ++i)
	{
		auto& partition = options.PartitionOptions[i];

		switch (partition.Format)
		{
		case EPartitionFormat::None:
			if (options.Verbose && !partition.Copies.empty())
				std::cout << "ImgGen WARN: copies to partition '" << (i + 1) << "' will be skipped, since the partition is unformatted\n";
			break;
		case EPartitionFormat::FAT:
		{
			FAT::State state       = FAT::LoadState(options, partition, outputFile);
			uint64_t   maxFileSize = FAT::MaxFileSize(state);
			bool       failed      = false;
			for (auto& fileCopy : partition.Copies)
			{
				for (auto itr : std::filesystem::recursive_directory_iterator { fileCopy.From, std::filesystem::directory_options::skip_permission_denied | std::filesystem::directory_options::follow_directory_symlink })
				{
					if (!itr.is_regular_file())
						continue;

					std::ifstream file { itr.path(), std::ios::binary | std::ios::ate };
					if (!file)
						continue;

					size_t fileSize = file.tellg();
					if (fileSize > maxFileSize)
					{
						file.close();
						std::cerr << "ImgGen ERROR: file " << itr.path() << " is too large for FAT partition '" << (i + 1) << "', will skip\n";
						continue;
					}
					file.seekg(0);

					std::string filepath = FAT::Normalize((fileCopy.To / std::filesystem::relative(itr.path(), fileCopy.From)).string());

					FAT::FileState fileState;
					if (FAT::Exists(state, filepath))
					{
						FAT::EFileType type = FAT::GetType(state, filepath);
						if (type != FAT::EFileType::File)
						{
							file.close();
							std::cerr << "ImgGen ERROR: file " << itr.path() << " already exists as a non file type, will skip\n";
							continue;
						}
						fileState          = FAT::GetFile(state, filepath);
						size_t currentSize = FAT::GetFileSize(fileState);
						if (currentSize == fileSize && FAT::GetModifyTime(fileState) == std::filesystem::last_write_time(itr.path()).time_since_epoch().count())
						{
							file.close();
							FAT::CloseFile(fileState);
							if (options.Verbose)
								std::cout << "ImgGen INFO: file " << itr.path() << " might be the same file, skipping\n";
							continue;
						}
					}
					else
					{
						fileState = FAT::CreateFile(state, filepath);
						if (options.Verbose)
							std::cout << "ImgGen INFO: file " << itr.path() << " created\n";
					}


					if (options.Verbose)
						std::cout << "ImgGen INFO: written 0/" << fileSize;
					FAT::EnsureFileSize(fileState, fileSize);
					FAT::SetModifyTime(fileState, std::filesystem::last_write_time(itr.path()).time_since_epoch().count());
					for (size_t i = 0; i < fileSize / 65536; ++i)
					{
						file.read((char*) transferBuffer, 65536);
						uint64_t writeCount = FAT::Write(fileState, transferBuffer, 65536);
						if (writeCount != 65536)
						{
							file.close();
							FAT::CloseFile(fileState);
							std::cerr << "\nImgGen ERROR: failed to write bytes after " << (i * 65536 + writeCount) << '\n';
							failed = true;
							break;
						}
						if (options.Verbose)
							std::cout << "\rImgGen INFO: written " << (i * 65536) << '/' << fileSize;
					}
					if (failed)
						break;
					if (fileSize % 65536 > 0)
					{
						file.read((char*) transferBuffer, fileSize % 65536);
						uint64_t writeCount = FAT::Write(fileState, transferBuffer, fileSize % 65536);
						if (writeCount != fileSize % 65536)
						{
							file.close();
							FAT::CloseFile(fileState);
							std::cerr << "\nImgGen ERROR: failed to write bytes after " << (fileSize / 65536 * 65536 + writeCount) << '\n';
							break;
						}
					}
					if (options.Verbose)
						std::cout << "\rImgGen INFO: written " << fileSize << '/' << fileSize << '\n';
					file.close();
					FAT::CloseFile(fileState);
				}
				if (failed)
					break;
			}
			FAT::SaveState(state);
			break;
		}
		}
	}
	delete[] transferBuffer;

	return 0;
}

bool ParseBoolean(std::string_view arg, bool def)
{
	if (CaselessStringCompare(arg, "false") == 0 ||
		CaselessStringCompare(arg, "n") == 0)
		return false;
	if (CaselessStringCompare(arg, "true") == 0 ||
		CaselessStringCompare(arg, "y") == 0)
		return true;
	return def;
}

int64_t ParseInt(std::string_view arg)
{
	return std::strtoll(arg.data(), nullptr, 10);
}

int64_t ParseByteCount(std::string_view arg)
{
	uint64_t value      = 0;
	bool     isNegative = arg.size() > 0 && arg[0] == '-';
	size_t   offset     = isNegative ? 1 : 0;
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

	size_t           suffixStart = std::min<size_t>(arg.size(), arg.find_first_not_of(' ', offset));
	size_t           suffixEnd   = std::min<size_t>(arg.size(), arg.find_first_of(' ', suffixStart + 1));
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
		value *= 1'000ULL;
	else if (CaselessStringCompare(suffix, "mb") == 0)
		value *= 1'000'000ULL;
	else if (CaselessStringCompare(suffix, "gb") == 0)
		value *= 1'000'000'000ULL;
	else if (CaselessStringCompare(suffix, "tb") == 0)
		value *= 1'000'000'000'000ULL;
	else if (CaselessStringCompare(suffix, "pb") == 0)
		value *= 1'000'000'000'000'000ULL;
	else if (CaselessStringCompare(suffix, "eb") == 0)
		value *= 1'000'000'000'000'000'000ULL;
	return isNegative ? -value : value;
}

uint64_t ParseSectorOffset(std::string_view arg, ESectorMode& mode)
{
	if (arg == ",")
	{
		mode = ESectorMode::Relative;
		return 0;
	}
	else if (arg == ".")
	{
		mode = ESectorMode::EndRelative;
		return 0;
	}

	if (arg.empty())
	{
		mode = ESectorMode::Relative;
		return 0;
	}

	if (arg[0] == '+')
	{
		mode = ESectorMode::Relative;
		return (ParseByteCount(arg.substr(1)) + 511) / 512;
	}
	else if (arg[0] == '-')
	{
		mode = ESectorMode::EndRelative;
		return (ParseByteCount(arg.substr(1)) + 511) / 512;
	}

	mode = ESectorMode::Absolute;
	return (ParseByteCount(arg) + 511) / 512;
}

EPartitionFormat ParsePartitionFormat(std::string_view arg)
{
	if (CaselessStringCompare(arg, "fat") == 0)
		return EPartitionFormat::FAT;
	return EPartitionFormat::None;
}

EPartitionScheme ParsePartitionScheme(std::string_view arg)
{
	if (CaselessStringCompare(arg, "mbr") == 0)
		return EPartitionScheme::MBR;
	if (CaselessStringCompare(arg, "gpt") == 0)
		return EPartitionScheme::GPT;
	return EPartitionScheme::GPT;
}