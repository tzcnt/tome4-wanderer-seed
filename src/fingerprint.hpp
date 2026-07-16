#pragma once

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "md5.h"

// Standard base64 encoding (matches LuaSocket's mime.b64).
inline std::string base64_encode(const unsigned char* data, size_t len) {
    static const char b64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(4 * ((len + 2) / 3));

    for (size_t i = 0; i < len; i += 3) {
        unsigned long v = static_cast<unsigned long>(data[i]) << 16;
        if (i + 1 < len) v |= static_cast<unsigned long>(data[i + 1]) << 8;
        if (i + 2 < len) v |= static_cast<unsigned long>(data[i + 2]);

        out += b64[(v >> 18) & 0x3f];
        out += b64[(v >> 12) & 0x3f];
        out += (i + 1 < len) ? b64[(v >> 6) & 0x3f] : '=';
        out += (i + 2 < len) ? b64[v & 0x3f] : '=';
    }
    return out;
}

// Compute the addons fingerprint matching WandererSeed.lua:
//
//   1. Start with {version_string}.
//   2. For each non-vanilla addon source, append "addonname-version".
//   3. Sort the list lexicographically.
//   4. Join with '|', MD5 hash, base64 encode.
//
// addon_versions maps addon source names to their version strings.
// The "@vanilla@" key, if present, is ignored.
inline std::string compute_fingerprint(
    const std::string& game_version,
    const std::unordered_set<std::string>& addon_sources,
    const std::unordered_map<std::string, std::string>& addon_versions) {

    std::vector<std::string> parts;
    parts.push_back(game_version);

    for (const auto& source : addon_sources) {
        if (source == "@vanilla@") continue;
        auto it = addon_versions.find(source);
        std::string ver = (it != addon_versions.end()) ? it->second : "???";
        parts.push_back(source + "-" + ver);
    }

    std::sort(parts.begin(), parts.end());

    std::string joined;
    for (size_t i = 0; i < parts.size(); i++) {
        if (i > 0) joined += '|';
        joined += parts[i];
    }

    char hash[HASHSIZE];
    md5(joined.c_str(), static_cast<long>(joined.size()), hash);

    return base64_encode(reinterpret_cast<const unsigned char*>(hash),
                         HASHSIZE);
}
