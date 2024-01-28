#pragma once

#include <cstdint>
namespace UTF
{
	struct CaseFold
	{
		uint32_t From;
		uint32_t To;
	};

	extern CaseFold s_CaseFolds[1457];
} // namespace UTF