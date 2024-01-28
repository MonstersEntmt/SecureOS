#include "KV.hpp"

#include <algorithm>
#include <cstdio>
#include <iostream>

namespace KV
{
	Option& Options::operator[](std::string_view name)
	{
		for (size_t i = 0; i < Options.size(); ++i)
		{
			if (Options[i].Spec->Name == name)
				return Options[i];
		}
		std::cerr << "ImgGen ERROR: attempting to get non existent option\n";
		exit(1);
	}

	void State::RegisterOption(OptionSpec option, HelpSpec help)
	{
		auto itr = std::find_if(Options.begin(), Options.end(), [&option](const std::pair<OptionSpec, HelpSpec>& opt) -> bool { return opt.first.Name == option.Name; });
		if (itr != Options.end())
		{
			std::cerr << "ImgGen ERROR: option '" << option.Name << "' already specified\n";
			exit(1);
		}
		Options.emplace_back(std::pair<OptionSpec, HelpSpec> { std::move(option), std::move(help) });
	}

	Options State::HandleArgs(const std::vector<std::string>& args)
	{
		struct Options options;
		options.Options.resize(Options.size());
		for (size_t i = 0; i < Options.size(); ++i)
			options.Options[i].Spec = &Options[i].first;
		for (auto& arg : args)
		{
			size_t keyEnd = std::min<size_t>(arg.size(), arg.find_first_of('='));

			std::string_view key = std::string_view { arg }.substr(0, keyEnd);
			size_t           i   = 0;
			for (; i < Options.size(); ++i)
			{
				if (Options[i].first.Name.starts_with(key))
					break;
			}
			if (i == Options.size())
			{
				std::cerr << "ImgGen ERROR: option '" << key << "' does not exist skipping\n";
				continue;
			}

			auto& optionOut = options.Options[i];
			if (optionOut.Present)
				std::cout << "ImgGen WARN: option '" << key << "' already specified overriding\n";
			optionOut.Present = true;
			if (keyEnd < arg.size())
				optionOut.Value = std::string_view { arg }.substr(keyEnd + 1);
		}
		for (size_t i = 0; i < Options.size(); ++i)
		{
			if (Options[i].first.Required && !options.Options[i].Present)
			{
				std::cerr << "ImgGen ERROR: option '" << Options[i].first.Name << "' is required\n";
				options.ShouldExit = true;
				options.ExitCode   = 1;
			}
		}
		return options;
	}

	void HelpFn(void* kvState, size_t indent)
	{
		if (!kvState)
		{
			std::cerr << "ImgGen ERROR: KV::HelpFn requires HelpSpec.HelpUserdata to be set\n";
			exit(1);
		}

		std::string indentStr(indent, ' ');

		State* state    = (State*) kvState;
		size_t padWidth = 0;
		for (auto& [optionSpec, helpSpec] : state->Options)
		{
			size_t headerSize = optionSpec.Name.size();
			if (helpSpec.Syntax.find_first_of('\n') >= helpSpec.Syntax.size())
				headerSize += helpSpec.Syntax.size();
			padWidth = std::max<size_t>(padWidth, headerSize);
		}
		std::string padStr(padWidth, ' ');
		for (auto& [optionSpec, helpSpec] : state->Options)
		{
			if (helpSpec.Syntax.find_first_of('\n') < helpSpec.Syntax.size())
			{
				std::printf("%s'%s'%.*s : %s\n", indentStr.c_str(), optionSpec.Name.c_str(), (int) (padWidth - optionSpec.Name.size()), padStr.c_str(), helpSpec.Description.c_str());
				size_t offset = 0;
				while (offset < helpSpec.Syntax.size())
				{
					size_t      lineEnd = std::min<size_t>(helpSpec.Syntax.size(), helpSpec.Syntax.find_first_of('\n', offset));
					std::string line    = helpSpec.Syntax.substr(offset, lineEnd - offset);
					offset              = std::min<size_t>(helpSpec.Syntax.size(), helpSpec.Syntax.find_first_not_of('\n', lineEnd + 1));
					std::printf("%s  %s\n", indentStr.c_str(), line.c_str());
				}
			}
			else
			{
				std::printf("%s'%s=%s'%.*s : %s\n", indentStr.c_str(), optionSpec.Name.c_str(), helpSpec.Syntax.c_str(), (int) (padWidth - optionSpec.Name.size() - helpSpec.Syntax.size()), padStr.c_str(), helpSpec.Description.c_str());
			}
			if (helpSpec.HelpFn)
				helpSpec.HelpFn(helpSpec.HelpUserdata, indent + 2);
		}
	}
} // namespace KV