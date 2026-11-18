#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace luthier::tests
{
// Shared by execution and --list, so a dry run describes the actual selection.
class TestSelection
{
public:
    explicit TestSelection (const std::vector<std::string>& arguments)
    {
        for (const auto& arg : arguments)
            if (arg == "--list" || arg == "-l")
                listOnly = true;
            else if (arg.compare (0, 7, "--skip=") == 0)
                excludedSuites.push_back (lower (arg.substr (7)));
            else if (arg.compare (0, 8, "--shard=") == 0)
                parseShard (arg.substr (8));
            else if (! arg.empty() && arg.front() != '-')
                filters.push_back (lower (arg));
    }

    bool includes (const std::string& suite, const std::string& name) const
    {
        const auto s = lower (suite);
        const auto n = lower (name);
        if (std::find (excludedSuites.begin(), excludedSuites.end(), s) != excludedSuites.end())
            return false;
        return filters.empty() || std::any_of (filters.begin(), filters.end(),
            [&] (const auto& filter)
            { return s.find (filter) != std::string::npos || n.find (filter) != std::string::npos; });
    }

    // --shard=i/n (1 <= i <= n): keep every n-th selected test, starting with
    // the i-th. The ordinal counts only tests that pass includes(), in registry
    // order, so the n shards of one binary partition its selection exactly and
    // spread long suites (Combo, CpuQuality) across CI jobs.
    bool inShard (int selectedOrdinal) const noexcept
    {
        return shardCount <= 1 || selectedOrdinal % shardCount == shardIndex - 1;
    }

    bool listOnly = false;
    bool valid = true;          // false when an argument (e.g. --shard) is malformed
    int shardIndex = 1;
    int shardCount = 1;

private:
    void parseShard (const std::string& spec)
    {
        const auto slash = spec.find ('/');
        const auto digits = [] (const std::string& t)
        { return ! t.empty() && t.size() <= 4 && std::all_of (t.begin(), t.end(), [] (unsigned char c) { return std::isdigit (c) != 0; }); };
        if (slash == std::string::npos || ! digits (spec.substr (0, slash)) || ! digits (spec.substr (slash + 1)))
        {
            valid = false;
            return;
        }
        const int index = std::stoi (spec.substr (0, slash));
        const int count = std::stoi (spec.substr (slash + 1));
        if (count < 1 || index < 1 || index > count)
        {
            valid = false;
            return;
        }
        shardIndex = index;
        shardCount = count;
    }

    static std::string lower (std::string text)
    {
        std::transform (text.begin(), text.end(), text.begin(),
            [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });
        return text;
    }
    std::vector<std::string> filters;
    std::vector<std::string> excludedSuites;
};
}
