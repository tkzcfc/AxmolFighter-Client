#include "SummaryManifest.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace mugen::legacy_cli
{
std::vector<ManifestEntry> loadManifest(const std::string& path)
{
    std::ifstream file(path);
    if (!file)
        throw std::runtime_error("Cannot open manifest: " + path);
    const auto slash      = path.find_last_of("/\\");
    const std::string dir = slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
    std::vector<ManifestEntry> entries;
    std::string line;
    int lineNo = 0;
    while (std::getline(file, line))
    {
        ++lineNo;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        std::istringstream row(line);
        ManifestEntry entry;
        std::string script;
        if (!(row >> entry.name) || entry.name[0] == '#')
            continue;
        if (!(row >> script >> entry.room >> entry.occupation >> entry.level >> entry.seed >> entry.frames))
            throw std::runtime_error("Invalid manifest line " + std::to_string(lineNo));
        entry.script = (!script.empty() && script[0] == '/') ? script : dir + script;
        entries.push_back(std::move(entry));
    }
    return entries;
}
}  // namespace mugen::legacy_cli
