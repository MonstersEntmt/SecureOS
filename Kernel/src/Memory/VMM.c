#include "Memory/VMM.h"

#include <string.h>

struct VMMImplementation
{
	const char* Name;
};

static const struct VMMImplementation s_VMMImplementations[] = {
	(struct VMMImplementation) {
								.Name = "freelut",
								}
};

static const struct VMMImplementation* s_VMMImpl = nullptr;

void VMMSelect(const char* name, size_t nameLen)
{
	for (size_t i = 0; !s_VMMImpl && i < sizeof(s_VMMImplementations) / sizeof(*s_VMMImplementations); ++i)
	{
		size_t implLen = strlen(s_VMMImplementations[i].Name);
		if (nameLen != implLen)
			continue;
		if (memcmp(name, s_VMMImplementations[i].Name, nameLen) == 0)
			s_VMMImpl = &s_VMMImplementations[i];
	}
	if (!s_VMMImpl)
		s_VMMImpl = &s_VMMImplementations[0];
}