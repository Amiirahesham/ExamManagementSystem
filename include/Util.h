#ifndef EMS_UTIL_H
#define EMS_UTIL_H

#include <string>
#include <vector>

#include "OptionalInt.h"

namespace ems {
namespace util {

// std::to_string / std::stoi are avoided on purpose: they are missing or broken on older MinGW builds.
std::string toString(long long value);
std::string trim(const std::string& s);
std::string toLower(std::string s);
std::string toUpper(std::string s);
std::vector<std::string> split(const std::string& s, char delimiter);   // keeps empty fields

// Strict integer parsing; throws ValidationException naming `field`.
int parseInt(const std::string& text, const std::string& field);
// Empty text or "null" (any case, as written by the workbook export) -> nullopt.
OptionalInt parseOptionalInt(const std::string& text, const std::string& field);
bool isNullToken(const std::string& text);
std::string optionalToField(const OptionalInt& value);

// Strict YYYY-MM-DD calendar validation (leap years included).
bool isValidDate(const std::string& text);

} // namespace util
} // namespace ems
#endif
