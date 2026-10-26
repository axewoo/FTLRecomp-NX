#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace ftl {
using Bytes = std::vector<unsigned char>;
std::uint32_t be32(const unsigned char* p);
std::uint16_t be16(const unsigned char* p);
// Archive lookups ignore ASCII case; this is the folded form they compare.
std::string archiveKey(std::string name);

class Archive {
    struct Entry {
        std::uint32_t offset, length, expanded;
        bool compressed;
    };

    std::filesystem::path path_;
    std::unordered_map<std::string, Entry> entries_;

public:
    explicit Archive(const std::filesystem::path& path);
    Bytes read(const std::string& name) const;

    bool contains(const std::string& name) const;
    // Every entry name, in archiveKey form.
    std::vector<std::string> names() const;

    std::size_t count() const {
        return entries_.size();
    }
};
} // namespace ftl
