// Quasizero Slicer — unit tests for the QZmini volumetric model. GNU AGPLv3.
#include "standalone/qz_test.hpp"
#include "libslic3r/QuasiZero/QzVolumetricModel.hpp"

using namespace Slic3r::QuasiZero;

QZ_TEST(barrel_area_35mm)
{
    QzVolumetricParams p;
    p.barrel_inner_diameter_mm = 35.0;
    QZ_CHECK_NEAR(p.barrel_area_mm2(), 962.113, 0.01); // pi*17.5^2
}

QZ_TEST(equations_match_specification)
{
    QzVolumetricParams p;
    p.barrel_inner_diameter_mm = 35.0;
    p.plunger_mm_per_e_unit = 0.277;
    const double area = QZ_PI * 17.5 * 17.5;
    QZ_CHECK_NEAR(p.effective_volume_mm3_per_e(), area * 0.277, 1e-9);
    QZ_CHECK_NEAR(p.equivalent_virtual_filament_diameter_mm(),
                  std::sqrt(4.0 * area * 0.277 / QZ_PI), 1e-9);
}

QZ_TEST(line_test_seed_values)
{
    // Preliminary test: 20mm x 4mm x 3mm at E0.9 -> ~266.7 mm3/E, ~0.277 mm/E, ~18.43 mm virtual diameter
    QzLineTest lt; // defaults are exactly that test
    QZ_CHECK_NEAR(lt.deposited_volume_mm3(), 240.0, 1e-9);
    QZ_CHECK_NEAR(lt.effective_volume_mm3_per_e(), 266.6667, 0.001);
    QzVolumetricParams p;
    p.plunger_mm_per_e_unit = lt.implied_plunger_mm_per_e(p.barrel_area_mm2());
    QZ_CHECK_NEAR(p.plunger_mm_per_e_unit, 0.2772, 0.0005);
    QZ_CHECK_NEAR(p.equivalent_virtual_filament_diameter_mm(), 18.43, 0.01);
}

QZ_TEST(e100_calibration)
{
    QZ_CHECK_NEAR(plunger_mm_per_e_from_e100(27.7), 0.277, 1e-12);
    QZ_CHECK_NEAR(plunger_mm_per_e_from_e100(13.85, 50.0), 0.277, 1e-12);
    QZ_CHECK_NEAR(plunger_mm_per_e_from_e100(10.0, 0.0), 0.0, 1e-12); // guarded
}

QZ_TEST(ml_e_roundtrip)
{
    QzVolumetricParams p;
    const double e = p.e_from_ml(120.0);
    QZ_CHECK_NEAR(p.ml_from_e(e), 120.0, 1e-9);
    QZ_CHECK(p.e_from_ml(120.0) > 0.0);
}

QZ_TEST(geometry_inconsistency_is_surfaced_not_reconciled)
{
    QzVolumetricParams p; // 35mm barrel, 140mm stroke, 150ml nominal -> ~134.7ml
    QZ_CHECK_NEAR(p.stroke_volume_ml(), 134.696, 0.01);
    auto w = p.consistency_warnings();
    bool found = false;
    for (auto& s : w) if (s.find("inconsistency") != std::string::npos) found = true;
    QZ_CHECK(found);
    // nominal capacity must remain 150, never silently derived from stroke
    QZ_CHECK_NEAR(p.nominal_syringe_capacity_ml, 150.0, 1e-12);
}

QZ_TEST(refill_count_formula)
{
    // refills = max(0, ceil(V/T) - 1)
    QZ_CHECK(QzMaterialBudget::refills_needed(0.0, 120.0) == 0);
    QZ_CHECK(QzMaterialBudget::refills_needed(100.0, 120.0) == 0);
    QZ_CHECK(QzMaterialBudget::refills_needed(120.0, 120.0) == 0);
    QZ_CHECK(QzMaterialBudget::refills_needed(120.1, 120.0) == 1);
    QZ_CHECK(QzMaterialBudget::refills_needed(240.0, 120.0) == 1);
    QZ_CHECK(QzMaterialBudget::refills_needed(240.1, 120.0) == 2);
    QZ_CHECK(QzMaterialBudget::refills_needed(500.0, 120.0) == 4);
    QZ_CHECK(QzMaterialBudget::refills_needed(100.0, 0.0) == 0); // guarded
}
