#include "archive_content.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sys/resource.h>

int main(int argc, char** argv) {
    assert(argc == 2);
    rusage baseline{};getrusage(RUSAGE_SELF,&baseline);
    const std::string root = argv[1];
    std::string error, temporary, directory;
    rr::ArchiveContent content;
    assert(content.prepare(root + "/raw.nes", "nes", false, error));
    assert(!content.extracted() && content.path() == root + "/raw.nes");
    assert(content.prepare(root + "/invalid.zip", "zip", true, error));
    assert(!content.extracted() && content.path() == root + "/invalid.zip");
    for (const auto* name : {"stored.zip", "deflated.ZIP", "nested.zip", "metadata.zip"}) {
        assert(content.prepare(root + "/" + name, "NES|sfc", false, error));
        assert(content.extracted());
        temporary = content.path();
        directory = std::filesystem::path(temporary).parent_path();
        assert(std::filesystem::exists(temporary));
        std::ifstream file(temporary, std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(file)), {});
        assert(bytes == "NES\x1a" "fake test content");
        content.reset();
        assert(!std::filesystem::exists(temporary));
        assert(!std::filesystem::exists(directory));
        content.reset(); // Idempotent cleanup.
    }
    for(const auto* extension:{"gb","gbc","gba","md","sms","gg","32x","a26","z64","j64","vb"}){
        assert(content.prepare(root+"/family-"+extension+".zip",extension,false,error));
        assert(content.extracted() && std::filesystem::path(content.path()).extension()==std::string(".")+extension);
        content.reset();
    }
    assert(content.prepare(root+"/large.zip","z64",false,error));
    assert(content.extracted() && std::filesystem::file_size(content.path())==64ULL*1024*1024);
    content.reset();
#ifdef __linux__
    rusage usage{};getrusage(RUSAGE_SELF,&usage);assert(usage.ru_maxrss<baseline.ru_maxrss+8*1024);
#endif
    assert(content.prepare(root+"/native-multifile.zip","zip|bin",true,error));
    assert(!content.extracted() && content.path()==root+"/native-multifile.zip");
    for (const auto* name : {"invalid.zip", "empty.zip", "no-match.zip", "multiple.zip",
                            "unsafe.zip", "corrupt.zip", "encrypted.zip", "oversized.zip"}) {
        assert(!content.prepare(root + "/" + name, "nes", false, error));
        assert(!error.empty() && !content.extracted());
    }
    assert(!content.prepare(root + "/disc.zip", "cue|bin", false, error));
    assert(!content.prepare(root + "/stored.zip", "nes", false, error,
                            (root + "/missing-directory").c_str()));
    assert(!content.extracted() && !error.empty());
    assert(content.prepare(root + "/stored.zip", "nes", false, error));
    temporary = content.path();
    directory = std::filesystem::path(temporary).parent_path();
    assert(content.prepare(root + "/raw.nes", "nes", false, error));
    assert(!std::filesystem::exists(directory));
    {
        rr::ArchiveContent owned;
        assert(owned.prepare(root + "/stored.zip", "nes", false, error));
        temporary = owned.path();
        directory = std::filesystem::path(temporary).parent_path();
    }
    assert(!std::filesystem::exists(directory));
    assert(std::filesystem::exists(root + "/stored.zip"));
    std::cout << "archive content tests passed\n";
}
