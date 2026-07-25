#include "config/config_file.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <system_error>

namespace rr::config
{
namespace
{

bool isSpace(unsigned char character)
{
    return std::isspace(character) != 0;
}

std::string trimCopy(const std::string &text)
{
    const auto first = std::find_if_not(text.begin(), text.end(),
                                        [](unsigned char character) {
                                            return isSpace(character);
                                        });
    if (first == text.end())
        return {};

    const auto last = std::find_if_not(text.rbegin(), text.rend(),
                                       [](unsigned char character) {
                                           return isSpace(character);
                                       }).base();
    return std::string(first, last);
}

bool isCommentMarker(char character)
{
    return character == '#' || character == ';';
}

std::size_t findInlineComment(const std::string &text)
{
    bool escaped = false;
    char quote = '\0';
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        const char character = text[index];
        if (escaped)
        {
            escaped = false;
            continue;
        }
        if (character == '\\' && quote != '\0')
        {
            escaped = true;
            continue;
        }
        if (quote != '\0')
        {
            if (character == quote)
                quote = '\0';
            continue;
        }
        if (character == '"' || character == '\'')
        {
            quote = character;
            continue;
        }
        if (isCommentMarker(character) &&
            (index == 0 || isSpace(static_cast<unsigned char>(text[index - 1]))))
            return index;
    }
    return std::string::npos;
}

bool decodeQuotedValue(const std::string &text, std::string &value,
                       std::string &error)
{
    const char quote = text.front();
    bool escaped = false;
    std::size_t index = 1;
    for (; index < text.size(); ++index)
    {
        const char character = text[index];
        if (escaped)
        {
            switch (character)
            {
            case 'n': value.push_back('\n'); break;
            case 'r': value.push_back('\r'); break;
            case 't': value.push_back('\t'); break;
            case '\\': value.push_back('\\'); break;
            case '"': value.push_back('"'); break;
            case '\'': value.push_back('\''); break;
            default:
                value.push_back('\\');
                value.push_back(character);
                break;
            }
            escaped = false;
            continue;
        }
        if (character == '\\')
        {
            escaped = true;
            continue;
        }
        if (character == quote)
            break;
        value.push_back(character);
    }

    if (escaped)
    {
        error = "unterminated escape sequence in quoted value";
        return false;
    }
    if (index == text.size())
    {
        error = "unterminated quoted value";
        return false;
    }

    const std::string remainder = trimCopy(text.substr(index + 1));
    if (!remainder.empty() && !isCommentMarker(remainder.front()))
    {
        error = "unexpected characters after quoted value";
        return false;
    }
    return true;
}

bool parseValue(const std::string &raw, std::string &value, std::string &error)
{
    const std::string trimmed = trimCopy(raw);
    if (trimmed.empty())
    {
        value.clear();
        return true;
    }

    if (trimmed.front() == '"' || trimmed.front() == '\'')
        return decodeQuotedValue(trimmed, value, error);

    const std::size_t comment = findInlineComment(trimmed);
    value = trimCopy(trimmed.substr(0, comment));
    return true;
}

std::string lowercase(const std::string &text)
{
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return result;
}

} // namespace

Document parse(std::istream &input)
{
    Document document;
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line))
    {
        ++lineNumber;
        if (lineNumber == 1 && line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xef &&
            static_cast<unsigned char>(line[1]) == 0xbb &&
            static_cast<unsigned char>(line[2]) == 0xbf)
            line.erase(0, 3);

        const std::string trimmed = trimCopy(line);
        if (trimmed.empty() || isCommentMarker(trimmed.front()))
            continue;

        const std::size_t separator = line.find('=');
        if (separator == std::string::npos)
        {
            document.diagnostics.push_back(
                {DiagnosticLevel::Error, lineNumber,
                 "missing '=' separator; entry ignored"});
            continue;
        }

        const std::string key = trimCopy(line.substr(0, separator));
        if (key.empty())
        {
            document.diagnostics.push_back(
                {DiagnosticLevel::Error, lineNumber,
                 "empty setting name; entry ignored"});
            continue;
        }
        if (std::any_of(key.begin(), key.end(), [](unsigned char character) {
                return !(std::isalnum(character) || character == '_' ||
                         character == '-' || character == '.');
            }))
        {
            document.diagnostics.push_back(
                {DiagnosticLevel::Error, lineNumber,
                 "invalid character in setting name '" + key + "'; entry ignored"});
            continue;
        }

        std::string value;
        std::string error;
        if (!parseValue(line.substr(separator + 1), value, error))
        {
            document.diagnostics.push_back(
                {DiagnosticLevel::Error, lineNumber,
                 error + "; setting '" + key + "' ignored"});
            continue;
        }

        const auto previous = document.source_lines.find(key);
        if (previous != document.source_lines.end())
        {
            document.diagnostics.push_back(
                {DiagnosticLevel::Warning, lineNumber,
                 "setting '" + key + "' overrides its value from line " +
                     std::to_string(previous->second)});
        }
        document.values[key] = value;
        document.source_lines[key] = lineNumber;
    }
    return document;
}

bool load(const std::string &path, Document &document, std::string &error)
{
    std::ifstream input(path);
    if (!input.good())
    {
        error = "unable to open configuration file";
        document = {};
        return false;
    }
    document = parse(input);
    error.clear();
    return true;
}

bool parseBoolean(const std::string &text, bool &value)
{
    const std::string normalized = lowercase(trimCopy(text));
    if (normalized == "true" || normalized == "enabled" ||
        normalized == "yes" || normalized == "on" || normalized == "1")
    {
        value = true;
        return true;
    }
    if (normalized == "false" || normalized == "disabled" ||
        normalized == "no" || normalized == "off" || normalized == "0")
    {
        value = false;
        return true;
    }
    return false;
}

bool parseInteger(const std::string &text, int minimum, int maximum, int &value)
{
    const std::string normalized = trimCopy(text);
    if (normalized.empty())
        return false;

    int parsed = 0;
    const char *begin = normalized.data();
    const char *end = begin + normalized.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc() || result.ptr != end ||
        parsed < minimum || parsed > maximum)
        return false;

    value = parsed;
    return true;
}

std::string encodeValue(const std::string &value)
{
    if (value.empty())
        return {};

    const bool boundaryWhitespace =
        isSpace(static_cast<unsigned char>(value.front())) ||
        isSpace(static_cast<unsigned char>(value.back()));
    bool requiresQuotes = boundaryWhitespace ||
                          isCommentMarker(value.front()) ||
                          value.find('\n') != std::string::npos ||
                          value.find('\r') != std::string::npos ||
                          value.find('\t') != std::string::npos ||
                          value.find('"') != std::string::npos ||
                          value.find('\\') != std::string::npos;
    for (std::size_t index = 1; !requiresQuotes && index < value.size(); ++index)
    {
        if (isCommentMarker(value[index]) &&
            isSpace(static_cast<unsigned char>(value[index - 1])))
            requiresQuotes = true;
    }
    if (!requiresQuotes)
        return value;

    std::string encoded;
    encoded.reserve(value.size() + 2);
    encoded.push_back('"');
    for (const char character : value)
    {
        switch (character)
        {
        case '\\': encoded += "\\\\"; break;
        case '"': encoded += "\\\""; break;
        case '\n': encoded += "\\n"; break;
        case '\r': encoded += "\\r"; break;
        case '\t': encoded += "\\t"; break;
        default: encoded.push_back(character); break;
        }
    }
    encoded.push_back('"');
    return encoded;
}

} // namespace rr::config
