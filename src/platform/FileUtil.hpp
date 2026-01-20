#pragma once

#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

namespace coomer {

inline bool readFileBytes(const std::string& path,
                          std::vector<unsigned char>& out, std::string* err) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        if (err) {
            *err = "failed to open file: " + path;
        }
        return false;
    }
    std::fseek(file, 0, SEEK_END);
    long size = std::ftell(file);
    if (size < 0) {
        std::fclose(file);
        if (err) {
            *err = "failed to stat file: " + path;
        }
        return false;
    }
    std::fseek(file, 0, SEEK_SET);
    out.resize(static_cast<size_t>(size));
    if (size > 0 && std::fread(out.data(), 1, static_cast<size_t>(size),
                               file) != static_cast<size_t>(size)) {
        std::fclose(file);
        if (err) {
            *err = "failed to read file: " + path;
        }
        return false;
    }
    std::fclose(file);
    return true;
}

inline int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

inline std::string urlDecode(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        char c = in[i];
        if (c == '%' && i + 2 < in.size()) {
            int hi = hexValue(in[i + 1]);
            int lo = hexValue(in[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        if (c == '+') {
            out.push_back(' ');
        } else {
            out.push_back(c);
        }
    }
    return out;
}

inline std::string fileUrlToPath(const std::string& uri) {
    const std::string prefix = "file://";
    if (uri.rfind(prefix, 0) == 0) {
        std::string path = uri.substr(prefix.size());
        const std::string localhost = "localhost";
        if (path.rfind(localhost, 0) == 0) {
            path = path.substr(localhost.size());
        }
        if (!path.empty() && path[0] != '/') {
            path.insert(path.begin(), '/');
        }
        return urlDecode(path);
    }
    return uri;
}

}  // namespace coomer
