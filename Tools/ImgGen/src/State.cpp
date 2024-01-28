#include "State.hpp"

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