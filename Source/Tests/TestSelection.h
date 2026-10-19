#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <stdexcept>
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

    // The same selection is used by --list and execution. Shards are 1-based.
    bool includes (const std::string& suite, const std::string& name) const
    {
        const auto s = lower (suite);
        const auto n = lower (name);
        if (std::find (excludedSuites.begin(), excludedSuites.end(), s) != excludedSuites.end())
            return false;
        if (! filters.empty() && ! std::any_of (filters.begin(), filters.end(),
            [&] (const auto& filter)
            { return s.find (filter) != std::string::npos || n.find (filter) != std::string::npos; }))
            return false;
        if (shardCount == 1)
            return true;

        // Platform-independent FNV-1a; std::hash is not stable across OSes.
        std::uint32_t hash = 2166136261u;
        const auto add = [&] (unsigned char byte)
        {
            hash = (hash ^ byte) * 16777619u;
        };
        for (unsigned char byte : s) add (byte);
        add (0);
        for (unsigned char byte : n) add (byte);
        return (hash % shardCount) == shardIndex;
    }

    bool listOnly = false;

private:
    void parseShard (const std::string& spec)
    {
        const auto slash = spec.find ('/');
        if (slash == std::string::npos || spec.find ('/', slash + 1) != std::string::npos)
            throw std::invalid_argument ("--shard must be N/M (1-based index)");
        const auto left = spec.substr (0, slash);
        const auto right = spec.substr (slash + 1);
        const auto digits = [] (const std::string& value)
        {
            return ! value.empty() && std::all_of (value.begin(), value.end(),
                [] (unsigned char c) { return std::isdigit (c) != 0; });
        };
        if (! digits (left) || ! digits (right))
            throw std::invalid_argument ("--shard requires numeric N/M");
        try
        {
            const auto index = std::stoul (left);
            const auto count = std::stoul (right);
            if (count == 0 || count > 128 || index == 0 || index > count)
                throw std::invalid_argument ("--shard index must be between 1 and M (max 128)");
            shardIndex = static_cast<std::uint32_t> (index - 1);
            shardCount = static_cast<std::uint32_t> (count);
        }
        catch (const std::out_of_range&)
        {
            throw std::invalid_argument ("--shard values are out of range");
        }
    }

    static std::string lower (std::string text)
    {
        std::transform (text.begin(), text.end(), text.begin(),
            [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });
        return text;
    }
    std::vector<std::string> filters;
    std::vector<std::string> excludedSuites;
    std::uint32_t shardIndex = 0;
    std::uint32_t shardCount = 1;
};
}
