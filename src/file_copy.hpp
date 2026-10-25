#pragma once
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <system_error>
#include <vector>

// std::filesystem::copy_file replacement. libstdc++'s version calls fchmod,
// which the Switch's newlib does not implement, so every copy failed there.
// A plain stream copy works everywhere. The destination is always replaced.
namespace ftl {
inline void copyFile(const std::filesystem::path& from,
                     const std::filesystem::path& to,
                     std::filesystem::copy_options = std::filesystem::copy_options::none) {
    std::ifstream input(from, std::ios::binary);
    if (!input) {
        throw std::runtime_error("copy_open_failed: " + from.string());
    }
    const std::vector<char> bytes{std::istreambuf_iterator<char>(input),
                                  std::istreambuf_iterator<char>()};
    std::ofstream output(to, std::ios::binary | std::ios::trunc);
    output.write(bytes.data(), std::streamsize(bytes.size()));
    output.close();
    if (!output) {
        throw std::runtime_error("copy_write_failed: " + to.string());
    }
}

inline void copyFile(const std::filesystem::path& from,
                     const std::filesystem::path& to,
                     std::filesystem::copy_options options,
                     std::error_code& error) noexcept {
    try {
        copyFile(from, to, options);
        error.clear();
    } catch (...) {
        error = std::make_error_code(std::errc::io_error);
    }
}
} // namespace ftl
