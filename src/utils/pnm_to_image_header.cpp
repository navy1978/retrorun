#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

bool read_token(std::istream& input, std::string& token) {
    while (input >> token) {
        if (!token.empty() && token[0] == '#') {
            input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        return true;
    }
    return false;
}

bool read_number(std::istream& input, int& value) {
    std::string token;
    if (!read_token(input, token)) return false;
    try {
        value = std::stoi(token);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " input.pnm output.h symbol\n";
        return 1;
    }

    std::ifstream input(argv[1], std::ios::binary);
    if (!input) {
        std::cerr << "Unable to open " << argv[1] << '\n';
        return 1;
    }

    std::string magic;
    int width = 0;
    int height = 0;
    int maximum = 0;
    if (!read_token(input, magic) || (magic != "P3" && magic != "P6") ||
        !read_number(input, width) || !read_number(input, height) ||
        !read_number(input, maximum) || width <= 0 || height <= 0 ||
        maximum <= 0 || maximum > 255) {
        std::cerr << "Unsupported PNM file\n";
        return 1;
    }

    // Image::pixel_data is sized for the existing 152x49 OSD assets.
    if (static_cast<size_t>(width) * height > 152U * 49U) {
        std::cerr << "Image exceeds the embedded OSD image capacity\n";
        return 1;
    }

    std::vector<uint8_t> rgb(static_cast<size_t>(width) * height * 3);
    if (magic == "P6") {
        input.get(); // consume the single whitespace byte after maxval
        input.read(reinterpret_cast<char*>(rgb.data()), rgb.size());
        if (input.gcount() != static_cast<std::streamsize>(rgb.size())) {
            std::cerr << "Truncated PNM pixel data\n";
            return 1;
        }
    } else {
        for (uint8_t& component : rgb) {
            int value = 0;
            if (!read_number(input, value) || value < 0 || value > maximum) {
                std::cerr << "Invalid PNM pixel data\n";
                return 1;
            }
            component = static_cast<uint8_t>(value * 255 / maximum);
        }
    }

    std::ofstream output(argv[2], std::ios::trunc);
    if (!output) {
        std::cerr << "Unable to create " << argv[2] << '\n';
        return 1;
    }

    output << "#pragma once\n\n#include \"globals.h\"\n\n";
    output << "// Generated from " << argv[1] << " by pnm_to_image_header.cpp.\n";
    output << "static Image " << argv[3] << " = {\n  " << width << ", " << height
           << ", 2,\n  \"";

    int bytes_on_line = 0;
    for (size_t i = 0; i < rgb.size(); i += 3) {
        const uint16_t pixel = static_cast<uint16_t>(((rgb[i] >> 3) << 11) |
                                                     ((rgb[i + 1] >> 2) << 5) |
                                                     (rgb[i + 2] >> 3));
        const uint8_t bytes[] = {static_cast<uint8_t>(pixel & 0xff),
                                 static_cast<uint8_t>(pixel >> 8)};
        for (uint8_t byte : bytes) {
            char escaped[5] = {};
            std::snprintf(escaped, sizeof(escaped), "\\%03o", byte);
            output << escaped;
            if (++bytes_on_line == 24 && i + 3 < rgb.size()) {
                output << "\"\n  \"";
                bytes_on_line = 0;
            }
        }
    }
    output << "\"\n};\n";
    return output.good() ? 0 : 1;
}
