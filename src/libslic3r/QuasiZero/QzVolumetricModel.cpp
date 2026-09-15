// Quasizero Slicer — QZmini volumetric model (implementation)
// Part of Quasizero Slicer, a fork of OrcaSlicer. GNU AGPLv3.
#include "QzVolumetricModel.hpp"

#include <sstream>
#include <iomanip>

namespace Slic3r { namespace QuasiZero {

std::vector<std::string> QzVolumetricParams::consistency_warnings() const
{
    std::vector<std::string> w;
    auto fmt = [](double v) {
        std::ostringstream os; os << std::fixed << std::setprecision(1) << v; return os.str();
    };
    // 35 mm barrel × 140 mm stroke ≈ 134.7 ml ≠ 150 ml nominal → surface it.
    if (usable_plunger_stroke_mm > 0.0 && nominal_syringe_capacity_ml > 0.0) {
        const double sv = stroke_volume_ml();
        const double rel = std::fabs(sv - nominal_syringe_capacity_ml) / nominal_syringe_capacity_ml;
        if (rel > 0.05)
            w.push_back("Geometry inconsistency: barrel " + fmt(barrel_inner_diameter_mm) +
                        " mm x stroke " + fmt(usable_plunger_stroke_mm) + " mm = " + fmt(sv) +
                        " ml, but nominal capacity is " + fmt(nominal_syringe_capacity_ml) +
                        " ml. Verify the real usable stroke and capacity; values are treated as independent.");
    }
    if (usable_syringe_capacity_ml > nominal_syringe_capacity_ml)
        w.push_back("Usable capacity exceeds nominal capacity; check configuration.");
    if (refill_threshold_ml > usable_syringe_capacity_ml)
        w.push_back("Refill threshold exceeds the configured usable capacity; no safety reserve remains.");
    if (plunger_mm_per_e_unit == 0.277)
        w.push_back("Plunger travel per E unit is the provisional line-test seed (0.277 mm/E). "
                    "Perform the E100 mechanical calibration before production use.");
    return w;
}

}} // namespace Slic3r::QuasiZero
