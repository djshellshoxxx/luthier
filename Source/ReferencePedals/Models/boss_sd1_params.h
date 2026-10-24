// OWNER: WP11
#pragma once
namespace luthier::reference::models
{
// Estimated values, except the secondary-sourced 1 MOhm drive pot and 47 nF
// stock gain-leg capacitor. This table is not a verified service schematic.
struct Sd1CircuitParams
{
    static constexpr double gainLegR = 4700, stockGainLegC = 47e-9, feedbackFixedR = 27000, drivePotR = 1e6,
                            feedbackC = 100e-12, gbw = 3e6, rail = 4.2, voltsPerFs = 2;
    struct Diode
    {
        double is, n;
    };
    static constexpr Diode diodes[3] = {{2.52e-9, 1.752}, {2e-7, 1.3}, {1e-12, 2.0}};
};
} // namespace luthier::reference::models
