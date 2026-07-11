// Quasizero Slicer — firmware adapters (implementation). GNU AGPLv3.
#include "QzFirmwareAdapter.hpp"

#include <algorithm>
#include <cctype>

namespace Slic3r { namespace QuasiZero {

static std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}

QzFirmwareFamily qz_family_from_flavor(const std::string &gcode_flavor)
{
    const std::string f = lower(gcode_flavor);
    if (f.find("klipper") != std::string::npos)  return QzFirmwareFamily::Klipper;
    if (f.find("reprap") != std::string::npos)   return QzFirmwareFamily::RepRapFirmware;
    if (f.find("marlin") != std::string::npos)   return QzFirmwareFamily::Marlin;
    return QzFirmwareFamily::Unknown;
}

const char *qz_family_name(QzFirmwareFamily f)
{
    switch (f) {
    case QzFirmwareFamily::Marlin:         return "Marlin";
    case QzFirmwareFamily::Klipper:        return "Klipper";
    case QzFirmwareFamily::RepRapFirmware: return "RepRapFirmware";
    case QzFirmwareFamily::PrusaBuddy:     return "Prusa/Buddy";
    case QzFirmwareFamily::BambuLab:       return "Bambu Lab";
    default:                               return "Unknown";
    }
}

const char *qz_family_validation_label(QzFirmwareFamily f)
{
    switch (f) {
    case QzFirmwareFamily::Marlin:         return "Offline G-code validated; hardware validation pending (Artillery Sidewinder X2)";
    case QzFirmwareFamily::Klipper:        return "Unverified; requires printer.cfg configuration (see QZMINI_REFILL_ASSIST.md)";
    case QzFirmwareFamily::RepRapFirmware: return "Unverified template";
    case QzFirmwareFamily::PrusaBuddy:     return "Unverified template";
    case QzFirmwareFamily::BambuLab:       return "EXPERIMENTAL / SLICE-ONLY; not hardware validated";
    default:                               return "Unverified";
    }
}

std::string qz_pause_command(QzFirmwareFamily family,
                             const std::string &strategy,
                             const std::string &machine_pause_gcode,
                             const std::string &custom_gcode)
{
    const std::string s = lower(strategy);
    if (s == "custom" && !custom_gcode.empty())
        return custom_gcode;
    if (s == "m0")   return "M0 ; QZ pause, wait for user";
    if (s == "m25")  return "M25 ; QZ pause SD/host print";
    if (s == "m600") return "M600 ; QZ pause via filament-change (validate resume behaviour first)";
    // auto
    if (!machine_pause_gcode.empty())
        return machine_pause_gcode;
    switch (family) {
    case QzFirmwareFamily::Klipper:        return "PAUSE ; QZ pause (requires PAUSE/RESUME macros in printer.cfg)";
    case QzFirmwareFamily::RepRapFirmware: return "M226 ; QZ pause (RepRapFirmware, unverified)";
    case QzFirmwareFamily::PrusaBuddy:     return "M601 ; QZ pause (Prusa, unverified)";
    case QzFirmwareFamily::BambuLab:       return "M400 U1 ; QZ pause (Bambu Lab, EXPERIMENTAL / SLICE-ONLY)";
    case QzFirmwareFamily::Marlin:
    default:                               return "M0 ; QZ pause, wait for user (Marlin)";
    }
}

std::string qz_family_comment(QzFirmwareFamily family)
{
    std::string c = "; QZ firmware family: ";
    c += qz_family_name(family);
    c += " — ";
    c += qz_family_validation_label(family);
    return c;
}

}} // namespace Slic3r::QuasiZero
