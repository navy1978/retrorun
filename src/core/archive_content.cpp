#include "archive_content.h"
#include "../../deps/minizip/unzip.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>
#include <unistd.h>

namespace rr {
namespace {
constexpr ZPOS64_T maxContentSize = 512ULL * 1024 * 1024;
constexpr ZPOS64_T maxEntries = 65536;

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return value;
}

std::string extension(const std::string& name) {
    const auto dot = name.find_last_of('.');
    const auto slash = name.find_last_of("/\\");
    return dot == std::string::npos || (slash != std::string::npos && dot < slash)
        ? "" : lower(name.substr(dot + 1));
}

bool allowed(const std::string& ext, const char* list) {
    if (ext.empty() || !list) return false;
    const std::string extensions = lower(list);
    std::size_t start = 0;
    while (start < extensions.size()) {
        auto end = extensions.find('|', start);
        if (end == std::string::npos) end = extensions.size();
        if (extensions.compare(start, end - start, ext) == 0) return true;
        start = end + 1;
    }
    return false;
}

bool safeMember(const std::string& name) {
    if (name.empty() || name.front() == '/' || name.find('\\') != std::string::npos ||
        name.find(':') != std::string::npos) return false;
    std::size_t start = 0;
    while (start < name.size()) {
        auto end = name.find('/', start);
        if (end == std::string::npos) end = name.size();
        if (name.compare(start, end - start, "..") == 0) return false;
        start = end + 1;
    }
    return true;
}

struct ZipCloser { void operator()(void* zip) const { if (zip) unzClose(zip); } };
using Zip = std::unique_ptr<void, ZipCloser>;
} // namespace

ArchiveContent::~ArchiveContent() { reset(); }

void ArchiveContent::reset() {
    if (!directory_.empty()) {
        if (!path_.empty()) unlink(path_.c_str());
        rmdir(directory_.c_str());
    }
    path_.clear();
    directory_.clear();
}

bool ArchiveContent::prepare(const std::string& originalPath,
                             const char* validExtensions, bool blockExtract,
                             std::string& error, const char* temporaryRoot) {
    reset();
    error.clear();
    path_ = originalPath;
    if (blockExtract || extension(originalPath) != "zip") return true;

    Zip zip(unzOpen64(originalPath.c_str()));
    if (!zip) { error = "Cannot open ZIP archive"; return false; }
    unz_global_info64 global{};
    if (unzGetGlobalInfo64(zip.get(), &global) != UNZ_OK ||
        global.number_entry == 0 || global.number_entry > maxEntries) {
        error = "Invalid or oversized ZIP directory";
        return false;
    }
    unz64_file_pos selected{};
    unz_file_info64 selectedInfo{};
    std::string selectedName;
    int result = unzGoToFirstFile(zip.get());
    for (ZPOS64_T index = 0; index < global.number_entry; ++index) {
        unz_file_info64 info{};
        if (result != UNZ_OK || unzGetCurrentFileInfo64(zip.get(), &info,
                nullptr, 0, nullptr, 0, nullptr, 0) != UNZ_OK ||
            info.size_filename == 0 || info.size_filename > 4096) {
            error = "Invalid ZIP member"; return false;
        }
        std::vector<char> nameBuffer(info.size_filename + 1, 0);
        if (unzGetCurrentFileInfo64(zip.get(), &info, nameBuffer.data(),
                nameBuffer.size(), nullptr, 0, nullptr, 0) != UNZ_OK ||
            std::strlen(nameBuffer.data()) != info.size_filename) {
            error = "Invalid ZIP filename"; return false;
        }
        const std::string name(nameBuffer.data());
        const auto slash = name.find_last_of('/');
        const std::string basename = name.substr(slash == std::string::npos ? 0 : slash + 1);
        // Ignore macOS metadata and directory entries, not actual ROM files.
        if (!basename.empty() && basename.front() != '.' &&
            name.rfind("__MACOSX/", 0) != 0 && allowed(extension(name), validExtensions)) {
            if (!safeMember(name)) { error = "Unsafe ZIP member path"; return false; }
            if (!selectedName.empty()) {
                error = "ZIP contains multiple compatible files; extract/select the game first";
                return false;
            }
            // Disc descriptors depend on companion files; extracting one alone is invalid.
            const auto ext = extension(name);
            if (ext == "cue" || ext == "gdi" || ext == "m3u") {
                error = "ZIP disc descriptors require manual extraction with companion files";
                return false;
            }
            if ((info.flag & 1) || info.uncompressed_size == 0 ||
                info.uncompressed_size > maxContentSize ||
                (info.compression_method != 0 && info.compression_method != Z_DEFLATED)) {
                error = "Encrypted, empty, oversized or unsupported ZIP content";
                return false;
            }
            selectedName = basename;
            selectedInfo = info;
            if (unzGetFilePos64(zip.get(), &selected) != UNZ_OK) {
                error = "Cannot locate ZIP content"; return false;
            }
        }
        if (index + 1 < global.number_entry) result = unzGoToNextFile(zip.get());
    }
    if (selectedName.empty()) { error = "ZIP has no file supported by this core"; return false; }
    if (unzGoToFilePos64(zip.get(), &selected) != UNZ_OK ||
        unzOpenCurrentFile(zip.get()) != UNZ_OK) {
        error = "Cannot decompress ZIP content"; return false;
    }

    const char* tmp = std::getenv("RETRORUN_EXTRACT_DIR");
    if (!tmp || !*tmp) tmp = temporaryRoot;
    if (!tmp || !*tmp) tmp = std::getenv("TMPDIR");
    std::string pattern = std::string(tmp && *tmp ? tmp : "/tmp") + "/retrorun-zip-XXXXXX";
    std::vector<char> directory(pattern.begin(), pattern.end());
    directory.push_back(0);
    if (!mkdtemp(directory.data())) {
        unzCloseCurrentFile(zip.get()); error = "Cannot create ZIP temporary directory"; return false;
    }
    directory_ = directory.data();
    path_ = directory_ + "/" + selectedName;
    // Fresh private directory (0700), exclusive file creation; never archive paths.
    FILE* output = std::fopen(path_.c_str(), "wbx");
    if (!output) {
        unzCloseCurrentFile(zip.get()); reset();
        error = "Cannot create temporary ROM"; return false;
    }
    std::array<unsigned char, 32768> buffer{};
    ZPOS64_T total = 0;
    bool ok = true;
    int bytes = 0;
    while ((bytes = unzReadCurrentFile(zip.get(), buffer.data(), buffer.size())) > 0) {
        total += static_cast<unsigned>(bytes);
        if (total > selectedInfo.uncompressed_size || total > maxContentSize ||
            std::fwrite(buffer.data(), 1, bytes, output) != static_cast<std::size_t>(bytes)) {
            ok = false; break;
        }
    }
    const int closeFile = std::fclose(output);
    const int closeZip = unzCloseCurrentFile(zip.get()); // Includes CRC verification.
    if (!ok || bytes < 0 || total != selectedInfo.uncompressed_size ||
        closeFile != 0 || closeZip != UNZ_OK) {
        reset(); error = "ZIP extraction failed (corrupt data, CRC or write error)"; return false;
    }
    return true;
}
} // namespace rr
