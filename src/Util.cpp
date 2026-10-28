#include "Util.h"

#include <cctype>
#include <climits>
#include <sstream>

#include "ExamException.h"

namespace ems {
namespace util {

std::string toString(long long value) {
    std::ostringstream os;
    os << value;
    return os.str();
}

std::string trim(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::string toLower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string toUpper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : s) {
        if (c == delimiter) { parts.push_back(current); current.clear(); }
        else current += c;
    }
    parts.push_back(current);
    return parts;
}

int parseInt(const std::string& text, const std::string& field) {
    const std::string t = trim(text);
    if (t.empty()) throw ValidationException("Field '" + field + "' is empty");
    const bool negative = t[0] == '-';
    const std::size_t first = negative ? 1 : 0;
    if (first == t.size()) throw ValidationException("Field '" + field + "' is not an integer: '" + t + "'");
    long long value = 0;
    for (std::size_t k = first; k < t.size(); ++k) {
        if (!std::isdigit(static_cast<unsigned char>(t[k])))
            throw ValidationException("Field '" + field + "' is not an integer: '" + t + "'");
        value = value * 10 + (t[k] - '0');
        if (value > 2147483648LL) throw ValidationException("Field '" + field + "' is out of range: '" + t + "'");
    }
    if (negative) value = -value;
    if (value < INT_MIN || value > INT_MAX) throw ValidationException("Field '" + field + "' is out of range: '" + t + "'");
    return static_cast<int>(value);
}

bool isNullToken(const std::string& text) {
    const std::string t = toLower(trim(text));
    return t.empty() || t == "null";
}

OptionalInt parseOptionalInt(const std::string& text, const std::string& field) {
    if (isNullToken(text)) return OptionalInt();
    return parseInt(text, field);
}

std::string optionalToField(const OptionalInt& value) {
    return value ? toString(*value) : std::string();
}

bool isValidDate(const std::string& t) {
    if (t.size() != 10 || t[4] != '-' || t[7] != '-') return false;
    for (std::size_t i = 0; i < t.size(); ++i) {
        if (i == 4 || i == 7) continue;
        if (!std::isdigit(static_cast<unsigned char>(t[i]))) return false;
    }
    auto num = [&t](std::size_t from, std::size_t len) {
        int v = 0;
        for (std::size_t i = from; i < from + len; ++i) v = v * 10 + (t[i] - '0');
        return v;
    };
    const int year = num(0, 4);
    const int month = num(5, 2);
    const int day = num(8, 2);
    if (year < 1900 || month < 1 || month > 12 || day < 1) return false;
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int limit = days[month - 1];
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    if (month == 2 && leap) limit = 29;
    return day <= limit;
}

} // namespace util
} // namespace ems
