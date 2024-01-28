#include "Args.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iostream>

namespace Args
{
	static std::vector<std::pair<OptionSpec, HelpSpec>> s_Options;

	static struct AutoRegister
	{
		AutoRegister()
		{
			RegisterOption({ .Name = "help", .MinArgs = 0, .MaxArgs = ~0ULL, .MaxCount = ~0ULL }, { .Syntax = "[option]", .Description = "Show this help info" });
			RegisterOption({ .Name = "version" }, { .Description = "Show version info in format 'FontGen major.minor.patch'" });
		}
	} s_AutoRegister;

	std::vector<std::string>& Option::operator[](size_t index)
	{
		if (index >= Options.size())
		{
			std::cerr << "FontGen ERROR: attempting to access non existent option '" << index << "'\n";
			exit(1);
		}
		return Options[index];
	}

	Option& Options::operator[](std::string_view name)
	{
		for (size_t i = 0; i < s_Options.size(); ++i)
		{
			if (s_Options[i].first.Name == name)
				return Options[i];
		}
		std::cerr << "FontGen ERROR: attempting to access non existent option '" << name << "'\n";
		exit(1);
	}

	void RegisterOption(OptionSpec option, HelpSpec help)
	{
		if (std::find_if(s_Options.begin(), s_Options.end(), [&option](const std::pair<OptionSpec, HelpSpec>& opt) -> bool { return opt.first.Name == option.Name; }) != s_Options.end())
		{
			std::cerr << "FontGen ERROR: option '" << option.Name << "' already specified\n";
			exit(1);
		}
		s_Options.emplace_back(std::pair<OptionSpec, HelpSpec> { std::move(option), std::move(help) });
	}

	Options Handle(int argc, const char* const* argv)
	{
		Options options;
		options.Options.resize(s_Options.size());
		size_t defaultArgI = s_Options.size();

		for (size_t i = 0; i < s_Options.size(); ++i)
		{
			options.Options[i].Options.reserve(s_Options[i].first.MinCount);
			if (s_Options[i].first.Name.empty())
			{
				options.Options[i].Options.resize(1);
				defaultArgI = i;
			}
		}

		for (size_t i = 1; i < argc; ++i)
		{
			std::string_view arg = argv[i];
			if (arg.empty())
				continue;

			size_t optionI         = 0;
			bool   isDefaultOption = false;
			if (arg[0] == '-')
			{
				if (arg.size() == 1)
				{
					std::cerr << "FontGen ERROR: invalid empty option '-' specified\n";
					options.ShouldExit = true;
					options.ExitCode   = 1;
					break;
				}
				if (arg[1] == '-')
				{
					if (arg.size() == 2)
						continue;
					arg = arg.substr(2);
					for (; optionI < s_Options.size(); ++optionI)
					{
						if (s_Options[optionI].first.Name == arg)
							break;
					}
					++i;
				}
				else
				{
					arg = arg.substr(1);
					for (; optionI < s_Options.size(); ++optionI)
					{
						if (s_Options[optionI].first.Name.starts_with(arg))
							break;
					}
					++i;
				}
			}
			else
			{
				isDefaultOption = true;
				optionI         = defaultArgI;
			}

			if (optionI == s_Options.size())
			{
				if (isDefaultOption)
					std::cerr << "FontGen ERROR: given argument for default option, but that is not allowed\n";
				else
					std::cerr << "FontGen ERROR: unknown option '" << arg << "' specified\n";
				options.ShouldExit = true;
				options.ExitCode   = 1;
				break;
			}

			auto& option = s_Options[optionI].first;

			if (!isDefaultOption && i > argc - option.MinArgs)
			{
				std::cerr << "FontGen ERROR: missing '" << (option.MinArgs - (argc - i - 1)) << "' arguments for option '--" << option.Name << "'\n";
				options.ShouldExit = true;
				options.ExitCode   = 1;
				break;
			}

			size_t argCount = 0;
			size_t firstArg = i;
			for (; i < argc; ++i)
			{
				std::string_view arg2 = argv[i];
				if (arg2.empty())
					continue;
				if (arg2[0] == '-')
				{
					--i;
					break;
				}
				++argCount;
			}

			if (!isDefaultOption)
			{
				if (argCount < option.MinArgs)
				{
					std::cerr << "FontGen ERROR: missing '" << (option.MinArgs - argCount) << "' arguments for option '--" << option.Name << "'\n";
					options.ShouldExit = true;
					options.ExitCode   = 1;
					break;
				}
				if (argCount > option.MaxArgs)
				{
					std::cerr << "FontGen ERROR: '" << (argCount - option.MaxArgs) << "' too many arguments for option '--" << option.Name << "'\n";
					options.ShouldExit = true;
					options.ExitCode   = 1;
					break;
				}
			}

			auto& optionOut = options.Options[optionI];
			if (isDefaultOption)
			{
				optionOut.Options[0].reserve(optionOut.Options[0].size() + argCount);
				for (size_t j = 0; j < argCount; ++j)
					optionOut.Options[0].emplace_back(argv[firstArg + j]);
			}
			else
			{
				std::vector<std::string> args;
				args.reserve(argCount);
				for (size_t j = 0; j < argCount; ++j)
					args.emplace_back(argv[firstArg + j]);
				optionOut.Options.emplace_back(args);
			}
		}

		if (!options.ShouldExit && options["help"])
		{
			for (auto& option : options["help"].Options)
			{
				if (option.empty())
				{
					std::cout << "FontGen Help\n"
								 "Syntax: [options...]";
					if (defaultArgI != s_Options.size())
						std::printf(" %s : %s\n", s_Options[defaultArgI].second.Syntax.c_str(), s_Options[defaultArgI].second.Description.c_str());
					else
						std::cout << '\n';
					std::cout << "Options:\n";
					size_t prePad  = 0;
					size_t postPad = 0;
					for (auto& [opt, help] : s_Options)
					{
						if (opt.Name.empty())
							continue;
						prePad  = std::max<size_t>(prePad, opt.Name.size());
						postPad = std::max<size_t>(postPad, help.Syntax.size());
					}
					for (auto& [opt, help] : s_Options)
					{
						if (opt.Name.empty())
							continue;
						std::printf("  --%-*s %-*s : %s\n", (int) prePad, opt.Name.c_str(), (int) postPad, help.Syntax.c_str(), help.Description.c_str());
					}
				}
				else
				{
					std::cout << "FontGen Help\n";
					size_t prePad  = 0;
					size_t postPad = 0;
					for (auto& opt : option)
					{
						auto itr = std::find_if(s_Options.begin(), s_Options.end(), [&opt](const std::pair<OptionSpec, HelpSpec>& spec) -> bool { return spec.first.Name.starts_with(opt); });
						if (itr != s_Options.end())
						{
							if (itr->first.Name.empty())
								continue;
							prePad  = std::max<size_t>(prePad, itr->first.Name.size());
							postPad = std::max<size_t>(postPad, itr->second.Syntax.size());
						}
					}
					for (auto& opt : option)
					{
						auto itr = std::find_if(s_Options.begin(), s_Options.end(), [&opt](const std::pair<OptionSpec, HelpSpec>& spec) -> bool { return spec.first.Name.starts_with(opt); });
						if (itr != s_Options.end())
						{
							std::printf("  --%-*s %-*s : %s\n", (int) prePad, itr->first.Name.c_str(), (int) postPad, itr->second.Syntax.c_str(), itr->second.Description.c_str());
							if (itr->second.HelpFn)
								itr->second.HelpFn(itr->second.HelpUserdata, 4);
						}
						else
						{
							std::cout << "FontGen WARN: help for option " << opt << " not found\n";
						}
					}
				}
			}
			options.ShouldExit = true;
			options.ExitCode   = 0;
		}
		else if (!options.ShouldExit && options["version"])
		{
			std::cout << "FontGen 1.0.0\n";
			options.ShouldExit = true;
			options.ExitCode   = 0;
		}
		else if (!options.ShouldExit && defaultArgI != s_Options.size())
		{
			size_t argCount = options.Options[defaultArgI].Options[0].size();
			if (argCount < s_Options[defaultArgI].first.MinArgs)
			{
				std::cerr << "FontGen ERROR: missing '" << (s_Options[defaultArgI].first.MinArgs - argCount) << "' arguments for default option\n";
				options.ShouldExit = true;
				options.ExitCode   = 1;
			}
			if (argCount > s_Options[defaultArgI].first.MaxArgs)
			{
				std::cerr << "FontGen ERROR: '" << (argCount - s_Options[defaultArgI].first.MaxArgs) << "' too many arguments for default option\n";
				options.ShouldExit = true;
				options.ExitCode   = 1;
			}
		}
		return options;
	}
} // namespace Args