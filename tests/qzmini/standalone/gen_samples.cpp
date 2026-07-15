// Quasizero Slicer — sample G-code generator for offline validation.
// Produces representative QZmini jobs (Artillery X2 / Marlin, cold extrusion)
// through the real QzRefillProcessor. These are synthetic fixtures for offline
// G-code validation; application-sliced samples are produced from the built
// Windows application. GNU AGPLv3.
#include "libslic3r/QuasiZero/QzRefillPlanner.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

using namespace Slic3r::QuasiZero;

static const char *MARLIN_START =
"; ===== QZmini cold-extrusion start (Quasizero Slicer, Marlin) — PROVISIONAL =====\n"
"G90 ; absolute XYZ\n"
"M83 ; relative E\n"
"M302 S0 ; allow cold extrusion (QZmini syringe drive)\n"
"M220 S100\nM221 S100\n"
"G28 ; home all axes (preserved)\n"
"G1 Z10 F600\n"
"G1 X10 Y10 F3000\n"
"G92 E0\n";

static const char *MARLIN_END =
"; QZmini cold end — PROVISIONAL\n"
"M400\nG92 E0\nG91\nG1 Z5 F600\nG90\nG1 X10 Y290 F3000\n"
"M106 S0\nM104 S0\nM140 S0\nM84\n";

static std::string body(const QzVolumetricParams &p, int layers, double ml_per_layer, int segments)
{
    std::ostringstream g;
    const double e_seg = p.e_from_ml(ml_per_layer) / segments;
    for (int l = 0; l < layers; ++l) {
        g << "; CHANGE_LAYER\n;AFTER_LAYER_CHANGE\n;" << (3.0 * (l + 1)) << "\n";
        g << "G1 Z" << (3.0 * (l + 1)) << " F600\n";
        for (int s = 0; s < segments; ++s) {
            g << "G0 X" << (20 + 4 * s) << " Y20 F3000\n";
            g << "; FEATURE: Internal solid infill\n";
            g << "G1 X" << (20 + 4 * s) << " Y" << (20 + 180) << " E" << e_seg << " F1200\n";
        }
    }
    return g.str();
}

static void emit(const std::string &path, const QzVolumetricParams &p, const QzRefillOptions &o,
                 const std::string &print_body, bool refill_enabled)
{
    std::ofstream f(path);
    f << "; Quasizero Slicer sample G-code (offline synthetic fixture)\n"
      << "; Printer: QZmini @ Artillery Sidewinder X2 3.0 nozzle (Marlin)\n"
      << "; Material: QZ Sawdust Biomaterial - Cold Extrusion\n"
      << "; STATUS: Offline G-code validated. NOT hardware validated.\n";
    f << MARLIN_START;
    if (refill_enabled) {
        QzRefillProcessor proc(p, o);
        std::string out = proc.process(print_body);
        if (proc.failed()) { std::cerr << "FAILED: " << proc.error() << "\n"; std::exit(2); }
        f << out;
        f << "; QZ total consumed: " << proc.consumed_ml() << " ml, refills: " << proc.events().size() << "\n";
        std::cout << path << " -> " << proc.consumed_ml() << " ml, " << proc.events().size() << " refill(s)\n";
    } else {
        f << print_body;
        std::cout << path << " -> no refill processing\n";
    }
    f << MARLIN_END;
}

int main()
{
    QzVolumetricParams p; p.plunger_mm_per_e_unit = 0.277;
    QzRefillOptions o;
    o.refill_threshold_ml = 120.0;
    o.usable_capacity_ml  = 134.7;
    o.park_x = 10; o.park_y = 10; o.park_z_lift = 20;
    o.plunger_reset_feedrate = 1800; // firm cold retract, not printing speed
    o.prime_after_refill = true; o.prime_ml = 1.0;
    o.family = QzFirmwareFamily::Marlin;
    o.pause_gcode = qz_pause_command(o.family, "auto", "M0 ; pause, wait for user", "");
    o.initial_e_relative = true; // QZ Marlin start G-code sets M83

    // 1) straight calibration line: 20 mm, E0.9 (preliminary test geometry)
    {
        std::ostringstream g;
        g << "; CHANGE_LAYER\nG1 Z3 F600\nG0 X100 Y100 F3000\n"
          << "M83\nG1 X120 Y100 E0.9 F1200 ; 20 mm calibration line (4 mm wide, 3 mm high)\n";
        emit("samples/qzmini/qz_sample_calibration_line.gcode", p, o, g.str(), true);
    }
    // 2) below threshold (~100 ml) — zero refills
    emit("samples/qzmini/qz_sample_no_refill.gcode",  p, o, body(p, 4, 25.0, 5), true);
    // 3) slightly above threshold (~130 ml) — one refill
    emit("samples/qzmini/qz_sample_one_refill.gcode", p, o, body(p, 5, 26.0, 5), true);
    // 4) above 240 ml (~260 ml) — two refills
    emit("samples/qzmini/qz_sample_two_refills.gcode", p, o, body(p, 10, 26.0, 5), true);
    return 0;
}
