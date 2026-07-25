#pragma once

#include <cstddef>
#include <istream>
#include <map>
#include <string>
#include <vector>

namespace rr::config
{

enum class DiagnosticLevel
{
    Warning,
    Error
};

struct Diagnostic
{
    DiagnosticLevel level = DiagnosticLevel::Error;
    std::size_t line = 0;
    std::string message;
};

struct Document
{
    std::map<std::string, std::string> values;
    std::map<std::string, std::size_t> source_lines;
    std::vector<Diagnostic> diagnostics;
};

// Parse RetroRun's intentionally small key/value format. Malformed entries are
// reported and skipped; valid entries remain available to the caller.
Document parse(std::istream &input);

// Open and parse a configuration file. Returns false only when the file cannot
// be opened. Syntax errors are returned in Document::diagnostics.
bool load(const std::string &path, Document &document, std::string &error);

// Strict, case-insensitive conversion helpers used by RetroRun settings.
// Core option strings are deliberately left untouched.
bool parseBoolean(const std::string &text, bool &value);
bool parseInteger(const std::string &text, int minimum, int maximum, int &value);

// Return a value that can be written back without changing its meaning when
// parsed again. Ordinary option labels remain unquoted for readability.
std::string encodeValue(const std::string &value);

} // namespace rr::config
