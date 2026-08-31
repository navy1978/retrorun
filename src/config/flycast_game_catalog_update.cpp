#include "config/flycast_game_catalog.h"

#include <curl/curl.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace rr::flycast_profiles
{
namespace
{

constexpr const char *RemoteCatalogUrl =
    "https://raw.githubusercontent.com/navy1978/retrorun/master/"
    "profiles/flycast2022-lowend/flycast-game-catalog.ini";
// The complete retail Product-number map plus device profiles exceeded the
// original 64 KiB ceiling. Keep a conservative finite limit while allowing
// the current signed-off catalog and future measured profiles to update.
constexpr std::size_t MaximumCatalogBytes = 256U * 1024U;
constexpr std::time_t CheckIntervalSeconds = 24 * 60 * 60;

struct DownloadBuffer
{
    std::string data;
};

std::size_t appendDownload(char *contents, std::size_t size,
                           std::size_t count, void *userdata)
{
    DownloadBuffer *buffer = static_cast<DownloadBuffer *>(userdata);
    const std::size_t bytes = size * count;
    if (!buffer || bytes > MaximumCatalogBytes ||
        buffer->data.size() > MaximumCatalogBytes - bytes)
        return 0;
    buffer->data.append(contents, bytes);
    return bytes;
}

bool downloadAndInstall(const std::string &cachePath, int currentVersion)
{
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
        return false;

    DownloadBuffer download;
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        curl_global_cleanup();
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, RemoteCatalogUrl);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
                     "RetroRun/3.2 Flycast-catalog-updater");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXFILESIZE_LARGE,
                     static_cast<curl_off_t>(MaximumCatalogBytes));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, appendDownload);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &download);

    const CURLcode result = curl_easy_perform(curl);
    long httpStatus = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    if (result != CURLE_OK || httpStatus != 200 || download.data.empty())
        return false;

    std::istringstream input(download.data);
    Catalog catalog;
    std::vector<std::string> diagnostics;
    if (!parseCatalog(input, RemoteCatalogUrl, catalog, diagnostics) ||
        catalog.catalog_version <= currentVersion)
        return false;

    namespace fs = std::filesystem;
    std::error_code error;
    const fs::path destination(cachePath);
    if (destination.has_parent_path())
        fs::create_directories(destination.parent_path(), error);
    if (error)
        return false;

    const fs::path temporary =
        destination.string() + ".tmp." + std::to_string(
#if defined(__unix__) || defined(__APPLE__)
            static_cast<long long>(getpid())
#else
            static_cast<long long>(std::time(nullptr))
#endif
        );
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output.good())
            return false;
        output.write(download.data.data(),
                     static_cast<std::streamsize>(download.data.size()));
        output.flush();
        if (!output.good())
        {
            output.close();
            fs::remove(temporary, error);
            return false;
        }
    }

    fs::rename(temporary, destination, error);
    if (error)
    {
        fs::remove(temporary, error);
        return false;
    }
    return true;
}

bool markCheckDue(const std::string &cachePath)
{
    const std::string stampPath = cachePath + ".last-check";
    const std::time_t now = std::time(nullptr);
#if defined(__unix__) || defined(__APPLE__)
    struct stat status = {};
    if (stat(stampPath.c_str(), &status) == 0 &&
        now >= status.st_mtime &&
        now - status.st_mtime < CheckIntervalSeconds)
        return false;
#endif

    namespace fs = std::filesystem;
    std::error_code error;
    const fs::path stamp(stampPath);
    if (stamp.has_parent_path())
        fs::create_directories(stamp.parent_path(), error);
    if (error)
        return false;
    std::ofstream output(stamp, std::ios::trunc);
    if (!output.good())
        return false;
    output << static_cast<long long>(now) << '\n';
    return output.good();
}

} // namespace

bool scheduleCatalogUpdate(const std::string &cache_path,
                           int current_catalog_version)
{
    if (cache_path.empty() || current_catalog_version <= 0 ||
        !markCheckDue(cache_path))
        return false;

#if defined(__unix__) || defined(__APPLE__)
    const pid_t intermediate = fork();
    if (intermediate < 0)
        return false;
    if (intermediate == 0)
    {
        const pid_t worker = fork();
        if (worker < 0)
            _exit(EXIT_FAILURE);
        if (worker > 0)
            _exit(EXIT_SUCCESS);

        setsid();
        const int nullDescriptor = open("/dev/null", O_RDWR);
        if (nullDescriptor >= 0)
        {
            dup2(nullDescriptor, STDIN_FILENO);
            dup2(nullDescriptor, STDOUT_FILENO);
            dup2(nullDescriptor, STDERR_FILENO);
            if (nullDescriptor > STDERR_FILENO)
                close(nullDescriptor);
        }
        const bool installed =
            downloadAndInstall(cache_path, current_catalog_version);
        _exit(installed ? EXIT_SUCCESS : EXIT_FAILURE);
    }

    int status = 0;
    while (waitpid(intermediate, &status, 0) < 0 && errno == EINTR)
    {
    }
    return true;
#else
    (void)current_catalog_version;
    return false;
#endif
}

} // namespace rr::flycast_profiles
