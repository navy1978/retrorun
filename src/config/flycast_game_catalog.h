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
    // False for title-only metadata entries that have never been tuned or
    // validated. Those entries identify retail variants but are not applied.
    bool validated = false;
    std::map<std::string, std::string> settings;
};

struct Catalog
{
    int schema_version = 0;
    int catalog_version = 0;
    std::string source;
    // Correctness-first base used when a device-specific validated profile is
    // layered over title-only metadata. Metadata-only products are not
    // selectable and therefore leave the user's configuration untouched.
    std::map<std::string, std::string> safe_defaults;
    std::map<std::string, std::map<Mode, Profile>> profiles;
    // Optional schema-v2 overrides keyed by normalized device name, then
    // product number and mode. Global profiles remain the compatibility base.
    std::map<std::string,
             std::map<std::string, std::map<Mode, Profile>>> device_profiles;
};

Mode parseMode(const std::string &value);
const char *modeName(Mode mode);
std::string normalizeProductNumber(const std::string &product_number);
std::string normalizeDeviceName(const std::string &device_name);

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
                   Mode mode, Profile &profile, bool &used_fallback,
                   const std::string &device_name = {});

// Return only products with an explicitly validated global or device profile.
// Title-only metadata baselines are intentionally excluded from the UI.
std::map<std::string, Profile> validatedCatalogProfiles(
    const Catalog &catalog);

// Catalogs store Flycast options in the canonical reicast_* namespace.
// Distribution builds that rename the core options can translate them here
// without changing catalog data or RetroRun-specific settings.
std::map<std::string, std::string> settingsForOptionPrefix(
    const std::map<std::string, std::string> &settings,
    const std::string &core_option_prefix);

} // namespace rr::flycast_profiles
