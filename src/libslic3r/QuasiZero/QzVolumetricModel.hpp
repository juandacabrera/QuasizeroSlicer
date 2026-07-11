// Quasizero Slicer — QZmini volumetric model
// Copyright (C) 2026 Quasizero
// This file is part of Quasizero Slicer, a fork of OrcaSlicer,
// released under GNU AGPLv3. Based on OrcaSlicer (SoftFever / OrcaSlicer contributors).
//
// Pure, dependency-free model of the QZmini syringe-plunger extrusion system.
// The printer firmware interprets QZmini plunger motion as E-axis movement;
// this model converts between commanded E units, physical plunger travel and
// deposited biomaterial volume.
#pragma once

#include <cmath>
#include <string>
#include <vector>
#include <cstdint>

namespace Slic3r { namespace QuasiZero {

constexpr double QZ_PI = 3.14159265358979323846;

struct QzVolumetricParams
{
    // --- syringe geometry (verify physically; see consistency_warnings) ---
    double barrel_inner_diameter_mm   = 35.0;
    double nominal_syringe_capacity_ml = 150.0; // manufacturer nominal, NOT derived
    double usable_syringe_capacity_ml  = 120.0; // configured safe usable volume per cycle
    double usable_plunger_stroke_mm    = 140.0; // reported, UNVERIFIED

    // --- mechanical calibration (primary calibration value) ---
    // Physical plunger travel produced by one commanded E unit.
    // Provisional seed derived from the preliminary 20 mm line test (E0.9).
    double plunger_mm_per_e_unit = 0.277;

    // --- final calibration multiplier (do not fold into geometry) ---
    double flow_ratio = 1.0;

    // --- refill strategy ---
    double refill_threshold_ml     = 120.0;
    double prime_after_refill_ml   = 1.0;

    // --- park / motion ---
    double park_x = 10.0;
    double park_y = 10.0;
    double park_z_lift = 20.0;
    double plunger_reset_feedrate = 300.0;  // mm/min of E axis (slow)
    double prime_feedrate         = 120.0;  // mm/min of E axis

    // ---- derived quantities (computed, never stored) ----
    double barrel_area_mm2() const {
        const double r = barrel_inner_diameter_mm / 2.0;
        return QZ_PI * r * r;
    }
    double effective_volume_mm3_per_e() const {
        return barrel_area_mm2() * plunger_mm_per_e_unit;
    }
    double e_units_per_plunger_mm() const {
        return plunger_mm_per_e_unit > 0.0 ? 1.0 / plunger_mm_per_e_unit : 0.0;
    }
    double equivalent_virtual_filament_diameter_mm() const {
        return std::sqrt(4.0 * effective_volume_mm3_per_e() / QZ_PI);
    }
    // volume conversions (flow_ratio applied where physical deposition matters)
    double ml_from_e(double e_units) const {
        return e_units * effective_volume_mm3_per_e() / 1000.0;
    }
    double e_from_ml(double ml) const {
        const double v = effective_volume_mm3_per_e();
        return v > 0.0 ? ml * 1000.0 / v : 0.0;
    }
    // theoretical volume of the reported stroke — used only for consistency checks
    double stroke_volume_ml() const {
        return barrel_area_mm2() * usable_plunger_stroke_mm / 1000.0;
    }

    // Never silently reconcile contradictory hardware values (project rule 9).
    std::vector<std::string> consistency_warnings() const;

    bool valid() const {
        return barrel_inner_diameter_mm > 0 && plunger_mm_per_e_unit > 0 &&
               usable_syringe_capacity_ml > 0 && refill_threshold_ml > 0;
    }
};

// ---- calibration helpers ----

// E100 test: command E100, measure physical plunger travel.
inline double plunger_mm_per_e_from_e100(double measured_plunger_travel_mm, double commanded_e = 100.0)
{
    return commanded_e > 0.0 ? measured_plunger_travel_mm / commanded_e : 0.0;
}

// Printed line test (rectangular approximation; provisional seed only):
// deposited_volume ≈ length × width × height; volume per E = volume / commanded E.
struct QzLineTest
{
    double path_length_mm = 20.0;
    double line_width_mm  = 4.0;
    double layer_height_mm = 3.0;
    double commanded_e     = 0.9;

    double deposited_volume_mm3() const { return path_length_mm * line_width_mm * layer_height_mm; }
    double effective_volume_mm3_per_e() const {
        return commanded_e > 0.0 ? deposited_volume_mm3() / commanded_e : 0.0;
    }
    // inverting the barrel geometry gives the implied plunger travel per E
    double implied_plunger_mm_per_e(double barrel_area_mm2) const {
        return barrel_area_mm2 > 0.0 ? effective_volume_mm3_per_e() / barrel_area_mm2 : 0.0;
    }
};

// ---- refill budgeting ----
struct QzMaterialBudget
{
    double model_volume_ml = 0.0;   // deposition into the model (+supports)
    double prime_purge_ml  = 0.0;   // skirt/prime/purge and refill primes
    double total_ml() const { return model_volume_ml + prime_purge_ml; }

    // refills = max(0, ceil(V / T) - 1); initial fill excluded
    static int refills_needed(double total_volume_ml, double refill_interval_ml) {
        if (refill_interval_ml <= 0.0 || total_volume_ml <= 0.0) return 0;
        int n = (int)std::ceil(total_volume_ml / refill_interval_ml) - 1;
        return n > 0 ? n : 0;
    }
};

}} // namespace Slic3r::QuasiZero
