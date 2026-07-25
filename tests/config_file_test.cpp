#include "config/config_file.h"

#include <cassert>
#include <sstream>
#include <string>
#include <vector>

namespace
{

void testCommentsWhitespaceAndCoreValues()
{
    std::istringstream input(
        "\xef\xbb\xbf# RetroRun configuration\r\n"
        " retrorun_auto_load = enabled \r\n"
        "; another comment\n"
        "flycast2021_alpha_sorting = per-strip (fast, least accurate)\n"
        "path = /roms/a#b/file.cdi # an inline comment\n");

    const auto document = rr::config::parse(input);
    assert(document.diagnostics.empty());
    assert(document.values.at("retrorun_auto_load") == "enabled");
    assert(document.values.at("flycast2021_alpha_sorting") ==
           "per-strip (fast, least accurate)");
    assert(document.values.at("path") == "/roms/a#b/file.cdi");
}

void testQuotedValuesAndEscapes()
{
    std::istringstream input(
        "empty =\n"
        "quoted = \" value # not a comment \" # comment\n"
        "single = 'a=b;still-a-value'\n"
        "escaped = \"line\\nnext\\tcolumn\"\n");

    const auto document = rr::config::parse(input);
    assert(document.diagnostics.empty());
    assert(document.values.at("empty").empty());
    assert(document.values.at("quoted") == " value # not a comment ");
    assert(document.values.at("single") == "a=b;still-a-value");
    assert(document.values.at("escaped") == "line\nnext\tcolumn");
}

void testMalformedEntriesAndDuplicates()
{
    std::istringstream input(
        "missing separator\n"
        "= empty-key\n"
        "bad key = ignored\n"
        "duplicate = first\n"
        "duplicate = second\n"
        "broken = \"unterminated\n"
        "valid = retained\n");

    const auto document = rr::config::parse(input);
    assert(document.diagnostics.size() == 5);
    assert(document.values.at("duplicate") == "second");
    assert(document.source_lines.at("duplicate") == 5);
    assert(document.values.at("valid") == "retained");
    assert(document.values.find("bad key") == document.values.end());
    assert(document.values.find("broken") == document.values.end());
}

void testTypedConversions()
{
    bool boolean = false;
    assert(rr::config::parseBoolean(" YES ", boolean) && boolean);
    assert(rr::config::parseBoolean("disabled", boolean) && !boolean);
    assert(!rr::config::parseBoolean("sometimes", boolean));

    int integer = 0;
    assert(rr::config::parseInteger(" 64 ", 0, 128, integer) && integer == 64);
    assert(!rr::config::parseInteger("64ms", 0, 128, integer));
    assert(!rr::config::parseInteger("-1", 0, 128, integer));
    assert(!rr::config::parseInteger("129", 0, 128, integer));
}

void testValueEncodingRoundTrip()
{
    using namespace rr::config;

    assert(encodeValue("per-strip (fast, least accurate)") ==
           "per-strip (fast, least accurate)");
    assert(encodeValue("plain#value") == "plain#value");
    assert(encodeValue("value # preserved") == "\"value # preserved\"");
    assert(encodeValue(" leading ") == "\" leading \"");

    const std::vector<std::string> values = {
        "", "plain", "value # preserved", "value ; preserved",
        " leading and trailing ", "quote \" and slash \\\\",
        "two\nlines"
    };
    for (const std::string &expected : values)
    {
        std::istringstream input("setting = " + encodeValue(expected) + "\n");
        const Document document = parse(input);
        assert(document.diagnostics.empty());
        assert(document.values.at("setting") == expected);
    }
}

} // namespace

int main()
{
    testCommentsWhitespaceAndCoreValues();
    testQuotedValuesAndEscapes();
    testMalformedEntriesAndDuplicates();
    testTypedConversions();
    testValueEncodingRoundTrip();
    return 0;
}
