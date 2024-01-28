#pragma once

#include <string>
#include <vector>

namespace Args
{
	struct OptionSpec
	{
		std::string Name;
		std::string Group    = "";
		uint64_t    MinArgs  = 0;
		uint64_t    MaxArgs  = 0;
		uint64_t    MinCount = 0;
		uint64_t    MaxCount = 1;
	};

	struct HelpSpec
	{
		std::string Syntax;
		std::string Description;

		void (*HelpFn)(void* userdata, size_t indent) = nullptr;
		void* HelpUserdata                            = nullptr;
	};

	struct Option
	{
		operator bool() const { return !Options.empty(); }

		std::vector<std::string>& operator[](size_t index);

		size_t Count() const { return Options.size(); }

		std::vector<std::vector<std::string>> Options;
	};

	struct Options
	{
		Option& operator[](std::string_view name);

		bool ShouldExit = false;
		int  ExitCode   = 0;

		std::vector<Option> Options;
	};

	void RegisterOption(OptionSpec option, HelpSpec help);

	Options Handle(int argc, const char* const* argv);
} // namespace Args