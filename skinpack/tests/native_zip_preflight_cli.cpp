#include "skinpack_zip_preflight.hpp"
#include <cstddef>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: skinpack_preflight <archive.rgsskin>\n";
        return 2;
    }
    std::ifstream f(argv[1], std::ios::binary);
    if (!f) return 3;
    std::vector<char> bytes{std::istreambuf_iterator<char>{f}, std::istreambuf_iterator<char>{}};
    const auto b = std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size());
    const auto result = rgsml::app::preflight_skinpack_zip(b);
    if (!result.ok) {
        std::cerr << "REJECT: " << result.reason << '\n';
        return 1;
    }
    std::cout << "ACCEPT " << result.entries.size() << " entries\n";
    return 0;
}
