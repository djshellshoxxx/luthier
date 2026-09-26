/*  The operating-system half of FirstRun::readOsPreferences (onboarding.md 5).

    Kept in a file of its own, with no JUCE header in it, so <windows.h> never
    meets juce_gui_basics in one translation unit: both declare things called
    Rectangle, and the collision is a compile error on some SDKs.

    Windows is the only platform read so far. macOS reports high contrast and
    reduced motion through NSWorkspace, which needs Objective-C++; until that
    file exists the answer there is "off", which is onboarding 5's default.
*/

#ifdef _WIN32
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>

 #ifdef _MSC_VER
  #pragma comment (lib, "user32.lib")
 #endif
#endif

namespace luthier
{
namespace FirstRunOs
{
    bool isHighContrastOn() noexcept
    {
       #ifdef _WIN32
        HIGHCONTRASTW contrast {};
        contrast.cbSize = sizeof (contrast);

        if (SystemParametersInfoW (SPI_GETHIGHCONTRAST, sizeof (contrast), &contrast, 0) != FALSE)
            return (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
       #endif

        return false;
    }

    bool isReducedMotionOn() noexcept
    {
       #ifdef _WIN32
        // "Show animations in Windows" off is the Windows form of reduced motion.
        BOOL animations = TRUE;

        if (SystemParametersInfoW (SPI_GETCLIENTAREAANIMATION, 0, &animations, 0) != FALSE)
            return animations == FALSE;
       #endif

        return false;
    }
}
} // namespace luthier
