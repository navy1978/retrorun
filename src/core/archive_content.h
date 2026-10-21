#pragma once

#include <string>

namespace rr {

// Owns only the temporary extracted file, never the original archive.
// Keep alive until the core has unloaded/deinitialized its content.
class ArchiveContent {
public:
    ArchiveContent() = default;
    ~ArchiveContent();
    ArchiveContent(const ArchiveContent&) = delete;
    ArchiveContent& operator=(const ArchiveContent&) = delete;

    // blockExtract leaves archive-aware cores in charge of the original ZIP.
    bool prepare(const std::string& originalPath, const char* validExtensions,
                 bool blockExtract, std::string& error,
                 const char* temporaryRoot = nullptr);
    void reset();
    const std::string& path() const { return path_; }
    bool extracted() const { return !directory_.empty(); }

private:
    std::string path_;
    std::string directory_;
};

} // namespace rr
