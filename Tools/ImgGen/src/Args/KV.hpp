#pragma once

#include <csignal>
#include <string>
#include <vector>

namespace KV
{
	struct OptionSpec
	{
		std::string Name;
		bool        Required = false;
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
		operator bool() const { return Present; }
		operator std::string_view() const { return Value; }

		bool        Present = false;
		std::string Value;
		OptionSpec* Spec = nullptr;
	};

	struct Options
	{
		Option& operator[](std::string_view name);

		bool ShouldExit = false;
		int  ExitCode   = 0;

		std::vector<Option> Options;
	};

	struct State
	{
		void    RegisterOption(OptionSpec option, HelpSpec help);
		Options HandleArgs(const std::vector<std::string>& args);

		std::vector<std::pair<OptionSpec, HelpSpec>> Options;
	};

	void HelpFn(void* kvState, size_t indent);
} // namespace KV