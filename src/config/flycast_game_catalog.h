#pragma once

#include <istream>
#include <map>
#include <string>
#include <vector>

namespace rr::flycast_profiles
{

enum class Mode
{
    Disabled,
    BestValidated,
    BestPerformance,
    Invalid
};

struct Profile
{
    std::string product_number;
    std::string title;
    Mode mode = Mode::Invalid;
    std::map<std::string, std::string> settings;
};

struct Catalog
{
    int schema_version = 0;
    int catalog_version = 0;
    std::string source;
    std::map<std::string, std::map<Mode, Profile>> profiles;
};

Mode parseMode(const std::string &value);
const char *modeName(Mode mode);
std::string normalizeProductNumber(const std::string &product_number);

bool parseCatalog(std::istream &input, const std::string &source,
                  Catalog &catalog, std::vector<std::string> &diagnostics);
bool loadCatalogFile(const std::string &path, Catalog &catalog,
                     std::vector<std::string> &diagnostics);
Catalog builtinCatalog();

// Returns the file RetroRun should inspect for a newer catalog. On Linux the
// actual /proc/self/exe location wins over argv[0].
std::string localCatalogPath(const char *argv0);
std::string cachedCatalogPath(const std::string &active_config_file);

// Schedule a non-blocking, at-most-daily HTTPS refresh. A successfully
// validated newer catalog is installed atomically and used on the next launch.
bool scheduleCatalogUpdate(const std::string &cache_path,
                           int current_catalog_version);

// best_performance falls back to best_validated when no separate aggressive
// profile exists for the selected product.
bool selectProfile(const Catalog &catalog, const std::string &product_number,
                   Mode mode, Profile &profile, bool &used_fallback);

} // namespace rr::flycast_profiles
