#pragma once

#include "GUID.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
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
	FAT
};

struct FileCopy
{
	std::filesystem::path From;
	std::string           To;
};

struct PartitionOptions
{
	ESectorMode StartMode = ESectorMode::Relative;
	ESectorMode EndMode   = ESectorMode::Relative;
	uint64_t    Start     = 0;
	uint64_t    End       = 0;

	uint64_t ActualStart = 0;
	uint64_t ActualEnd   = 0;

	union
	{
		GUID    guid;
		uint8_t iden;
	} Type = {};
	std::string Name;

	EPartitionFormat Format        = EPartitionFormat::None;
	bool             ForceReformat = false;

	std::vector<FileCopy> Copies;
};

enum class EPartitionScheme : uint8_t
{
	MBR,
	GPT
};

struct ImgGenOptions
{
	bool Verbose        = false;
	bool CanExpand      = true;
	bool RetainBootCode = false;

	std::filesystem::path OutputFilepath      = "drive.img";
	int64_t               ImageSize           = 0;
	int64_t               PhysicalSize        = 0;
	int64_t               TransferGranularity = 0;
	uint8_t               PartitionCount      = 0;

	EPartitionScheme PartitionScheme = EPartitionScheme::GPT;

	PartitionOptions PartitionOptions[128];
};

int CaselessStringCompare(std::string_view lhs, std::string_view rhs);