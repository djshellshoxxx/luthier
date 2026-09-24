#pragma once

/*  Where Help sends people: the homepage, the source and the support address
    (include.md "Help File"; TODO 13, docs/spec-coverage.md INC-HLP-02).
    TUNE-HELP-ONBOARDING workstream.

    THE ONE PLACE TO CONFIGURE THEM. Every surface that shows or opens a
    support link - the HELP tab and overlay, the About topic, the crash-report
    note in the debug window - reads these constants and nothing else.

    This build does not know the publisher's real addresses, so the defaults
    are reserved example names (RFC 2606) that can never reach a real third
    party. A release build sets its own at configure time, without touching
    source:

        cmake -DCMAKE_CXX_FLAGS="-DLUTHIER_HOMEPAGE_URL=\\\"https://...\\\" \
                                 -DLUTHIER_SOURCE_URL=\\\"https://...\\\" \
                                 -DLUTHIER_SUPPORT_EMAIL=\\\"help@...\\\""

    or by editing the three defaults below. areConfigured() stays false while
    any placeholder remains, so a release check (and the About topic, which
    says so) can tell.
*/

namespace luthier
{
namespace SupportLinks
{
   #ifndef LUTHIER_HOMEPAGE_URL
    #define LUTHIER_HOMEPAGE_URL "https://luthieraudio.example/luthier"
   #endif

   #ifndef LUTHIER_SOURCE_URL
    #define LUTHIER_SOURCE_URL "https://git.luthieraudio.example/luthier"
   #endif

   #ifndef LUTHIER_SUPPORT_EMAIL
    #define LUTHIER_SUPPORT_EMAIL "support@luthieraudio.example"
   #endif

    inline constexpr const char* homepageUrl  = LUTHIER_HOMEPAGE_URL;
    inline constexpr const char* sourceUrl    = LUTHIER_SOURCE_URL;
    inline constexpr const char* supportEmail = LUTHIER_SUPPORT_EMAIL;

    /** The reserved name the placeholders use. */
    inline constexpr const char* placeholderDomain = ".example";

    /** False while any of the three is still a placeholder. */
    constexpr bool areConfigured() noexcept
    {
        auto isPlaceholder = [] (const char* s)
        {
            // Looks for ".example" followed by '/', end or anything but a letter.
            for (const char* p = s; *p != 0; ++p)
            {
                const char* a = p;
                const char* b = placeholderDomain;

                while (*b != 0 && *a == *b) { ++a; ++b; }

                if (*b == 0 && ! ((*a >= 'a' && *a <= 'z') || (*a >= 'A' && *a <= 'Z')))
                    return true;
            }

            return false;
        };

        return ! isPlaceholder (homepageUrl) && ! isPlaceholder (sourceUrl) && ! isPlaceholder (supportEmail);
    }
}
} // namespace luthier
