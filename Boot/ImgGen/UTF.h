#pragma once

#include <string>
#include <string_view>

std::u16string UTF8ToUTF16(std::string_view utf8);
std::string    UTF16ToUTF8(std::u16string_view utf16);