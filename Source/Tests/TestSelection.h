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

    bool listOnly = false;

private:
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
