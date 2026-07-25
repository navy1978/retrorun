#include "file_browser.h"

#include "disk_control.h"
#include "fonts.h"
#include "platform.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace {
struct Entry { std::string name; fs::path path; bool directory; };
bool visible = false;
bool just_opened = false;
fs::path directory;
std::vector<Entry> entries;
size_t selected = 0;
std::string message;

void reload() {
    entries.clear(); selected = 0; message.clear();
    std::error_code ec;
    if (directory.has_parent_path()) entries.push_back({"..", directory.parent_path(), true});
    for (fs::directory_iterator it(directory, fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec)) {
        const bool is_dir = it->is_directory(ec);
        if (!is_dir && !it->is_regular_file(ec)) continue;
        entries.push_back({it->path().filename().string(), it->path(), is_dir});
    }
    std::sort(entries.begin() + (!entries.empty() && entries[0].name == ".." ? 1 : 0), entries.end(),
              [](const Entry& a, const Entry& b) {
                  if (a.directory != b.directory) return a.directory > b.directory;
                  return a.name < b.name;
              });
    if (ec) message = ec.message();
}
}

bool rr_file_browser_visible() { return visible; }
void rr_file_browser_close() { visible = false; }
void rr_file_browser_open(const std::string& initial_path) {
    fs::path p(initial_path);
    std::error_code ec;
    if (!fs::is_directory(p, ec)) p = p.parent_path();
    if (p.empty() || !fs::is_directory(p, ec)) p = fs::current_path(ec);
    directory = p; visible = true; just_opened = true; reload();
}

void rr_file_browser_input(bool up, bool down, bool accept, bool cancel) {
    if (!visible) return;
    if (just_opened) { just_opened = false; return; }
    if (cancel) { visible = false; return; }
    if (entries.empty()) return;
    if (up) selected = (selected + entries.size() - 1) % entries.size();
    if (down) selected = (selected + 1) % entries.size();
    if (!accept) return;
    const Entry entry = entries[selected];
    if (entry.directory) { directory = entry.path; reload(); return; }
    if (rr_disk_control_add_and_select(entry.path.string(), &message)) {
        message = "Disk inserted: " + entry.name;
        visible = false;
    }
}

void rr_file_browser_render(rr_surface* surface, int width, int height) {
    if (!surface) return;
    uint16_t* pixels = static_cast<uint16_t*>(rr_surface_map(surface));
    const int stride = rr_surface_stride_get(surface) / 2;
    std::fill(pixels, pixels + stride * height, static_cast<uint16_t>(0x0841));
    basic_text_out16_nf_color_clipped(pixels, stride, width, height, 8, 7, "SELECT DISK IMAGE", 0xffff);
    std::string path = directory.string();
    if (path.size() > static_cast<size_t>((width - 16) / 6)) path = "..." + path.substr(path.size() - (width - 34) / 6);
    basic_text_out16_nf_color_clipped(pixels, stride, width, height, 8, 19, path.c_str(), 0x7bef);
    const int first = selected > 5 ? static_cast<int>(selected) - 5 : 0;
    const int rows = std::max(1, (height - 58) / 11);
    for (int line = 0; line < rows && first + line < static_cast<int>(entries.size()); ++line) {
        const int index = first + line;
        std::string label = entries[index].directory ? "[" + entries[index].name + "]" : entries[index].name;
        const size_t max_chars = std::max(1, (width - 22) / 6);
        if (label.size() > max_chars) label.resize(max_chars);
        basic_text_out16_nf_color_clipped(pixels, stride, width, height, 8, 35 + line * 11,
                                         label.c_str(), index == static_cast<int>(selected) ? 0xffe0 : 0xffff);
    }
    const char* help = message.empty() ? "D-PAD Move  A Select  B Close" : message.c_str();
    basic_text_out16_nf_color_clipped(pixels, stride, width, height, 8, height - 13, help, message.empty() ? 0xbdf7 : 0xf800);
    rr_surface_unmap(surface);
}
