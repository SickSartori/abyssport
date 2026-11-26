// A crash leaves abyss-crash.dmp in the logs folder (src/platform/platform_win32.c), and the next start would overwrite
// abyss.log. So the launcher, when it opens or the game comes back, puts both in abyss-crash-report.zip, ready to attach
// to a report. As mc3port's (xport, src/pc/crash_report.cpp).
#pragma once

#include <windows.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace crash_report {

constexpr uint32_t crc32(const char* data, size_t size) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) {
        crc ^= static_cast<uint8_t>(data[i]);
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
    }
    return ~crc;
}
static_assert(crc32("123456789", 9) == 0xCBF43926u);

inline bool read_file(const std::filesystem::path& path, std::string& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return !file.bad();
}

inline void put(std::string& out, uint32_t value, int bytes) {
    for (int i = 0; i < bytes; ++i) out += static_cast<char>(value >> (8 * i) & 0xFF);
}

// shortcut: entries are stored, not compressed, and no larger than 4 GB; a dump is small enough for GitHub's 25 MB
// so far. Deflate only if reports grow past that.
inline std::string zip_of(const std::string* const* contents, const char* const* names, int count) {
    SYSTEMTIME now{};
    GetLocalTime(&now);
    const uint32_t time = (now.wHour << 11) | (now.wMinute << 5) | (now.wSecond / 2);
    const uint32_t date = ((now.wYear - 1980) << 9) | (now.wMonth << 5) | now.wDay;
    std::string zip, directory;
    for (int i = 0; i < count; ++i) {
        const std::string name = names[i];
        const std::string& data = *contents[i];
        const uint32_t size = static_cast<uint32_t>(data.size());
        const uint32_t crc = crc32(data.data(), data.size());
        const auto common = [&](std::string& out) {  // the part both headers share
            put(out, 20, 2), put(out, 0, 2), put(out, 0, 2);  // version needed, flags, stored
            put(out, time, 2), put(out, date, 2), put(out, crc, 4), put(out, size, 4), put(out, size, 4);
            put(out, static_cast<uint32_t>(name.size()), 2), put(out, 0, 2);  // name's length, no extra field
        };
        const uint32_t offset = static_cast<uint32_t>(zip.size());
        put(zip, 0x04034B50, 4);
        common(zip);
        zip += name;
        zip += data;
        put(directory, 0x02014B50, 4), put(directory, 20, 2);  // made by
        common(directory);
        put(directory, 0, 2), put(directory, 0, 2), put(directory, 0, 2), put(directory, 0, 4);  // comment, disk, attributes
        put(directory, offset, 4);
        directory += name;
    }
    const uint32_t directory_offset = static_cast<uint32_t>(zip.size());
    zip += directory;
    put(zip, 0x06054B50, 4), put(zip, 0, 4);  // end of the directory, one disk
    put(zip, count, 2), put(zip, count, 2), put(zip, static_cast<uint32_t>(directory.size()), 4);
    put(zip, directory_offset, 4), put(zip, 0, 2);
    return zip;
}

// The crash in a few lines, for the issue: main.c's "== crash: ..." and the first return addresses under it.
inline std::string summary_of(const std::string& log) {
    const size_t start = log.find("== crash: ");
    if (start == std::string::npos) return {};
    std::string summary;
    size_t line = start;
    for (int kept = 0; line < log.size() && kept < 6; ++kept) {
        size_t end = log.find('\n', line);
        if (end == std::string::npos) end = log.size();
        std::string text = log.substr(line, end - line);
        if (!text.empty() && text.back() == '\r') text.pop_back();
        if (line != start && text.rfind("  ", 0) != 0) break;  // the backtrace ends at the next unindented line
        if (text.size() > 200) text.resize(200);  // the issue travels in the address: keep it short
        summary += text + "\n";
        line = end + 1;
    }
    return summary;
}

inline std::filesystem::path path_in(const std::filesystem::path& logs) { return logs / "abyss-crash-report.zip"; }

// True if a report was made: the dump is then taken away, so it is made once. `summary` gets the crash's lines.
inline bool make(const std::filesystem::path& logs, std::string& summary) {
    try {
        const std::filesystem::path dump_path = logs / "abyss-crash.dmp", log_path = logs / "abyss.log";
        std::error_code error;
        const auto dump_time = std::filesystem::last_write_time(dump_path, error);
        if (error) return false;
        const auto log_time = std::filesystem::last_write_time(log_path, error);
        // a later run's log with an old dump: not this crash's
        if (error || log_time - dump_time > std::chrono::minutes(10)) return false;
        std::string log, dump;
        if (!read_file(log_path, log) || !read_file(dump_path, dump)) return false;
        const std::string* const contents[] = {&log, &dump};
        const char* const names[] = {"abyss.log", "abyss-crash.dmp"};
        const std::string zip = zip_of(contents, names, 2);
        std::filesystem::path temporary = path_in(logs);
        temporary += ".tmp";
        {
            std::ofstream file(temporary, std::ios::binary);
            file.write(zip.data(), static_cast<std::streamsize>(zip.size()));
            file.close();
            if (!file) return false;
        }
        std::filesystem::rename(temporary, path_in(logs));
        std::filesystem::remove(dump_path, error);
        summary = summary_of(log);
        return true;
    } catch (const std::exception&) {  // out of memory or a file in use: the dump stays for the next time
        return false;
    }
}

}  // namespace crash_report
