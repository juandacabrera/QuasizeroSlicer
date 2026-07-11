// Quasizero Slicer — refill planner unit + golden tests. GNU AGPLv3.
#include "standalone/qz_test.hpp"
#include "libslic3r/QuasiZero/QzRefillPlanner.hpp"

#include <sstream>

using namespace Slic3r::QuasiZero;

namespace {

QzVolumetricParams test_params()
{
    QzVolumetricParams p;                 // 35 mm barrel
    p.plunger_mm_per_e_unit = 0.277;      // ≈266.5 mm3/E  => 1 ml ≈ 3.7523 E
    return p;
}

QzRefillOptions test_options()
{
    QzRefillOptions o;
    o.refill_threshold_ml = 120.0;
    o.usable_capacity_ml  = 120.0 + 14.7; // 134.7 usable => ~14.7 ml reserve
    o.park_x = 5; o.park_y = 5; o.park_z_lift = 20;
    o.pause_gcode = qz_pause_command(QzFirmwareFamily::Marlin, "auto", "", "");
    return o;
}

// Build a synthetic print: N layers, each depositing 'ml_per_layer' in 'segments'
// extrusions separated by travels, relative E mode.
std::string synth_print(const QzVolumetricParams &p, int layers, double ml_per_layer, int segments = 4)
{
    std::ostringstream g;
    g << "G90\nM83\nG28\nG92 E0\n";
    const double e_seg = p.e_from_ml(ml_per_layer) / segments;
    for (int l = 0; l < layers; ++l) {
        g << "; CHANGE_LAYER\n";
        g << "G1 Z" << (3.0 * (l + 1)) << " F600\n";
        for (int s = 0; s < segments; ++s) {
            g << "G0 X" << (10 + s) << " Y10 F3000\n";
            g << "G1 X" << (10 + s) << " Y100 E" << e_seg << " F1200\n";
        }
    }
    return g.str();
}

int count_occurrences(const std::string &s, const std::string &sub)
{
    int n = 0; size_t pos = 0;
    while ((pos = s.find(sub, pos)) != std::string::npos) { ++n; pos += sub.size(); }
    return n;
}

} // namespace

QZ_TEST(no_refill_below_threshold)
{
    auto p = test_params(); auto o = test_options();
    QzRefillProcessor proc(p, o);
    std::string out = proc.process(synth_print(p, 4, 25.0)); // 100 ml total
    QZ_CHECK(proc.events().empty());
    QZ_CHECK(!proc.failed());
    QZ_CHECK(count_occurrences(out, "QZ_REFILL_BEGIN") == 0);
    QZ_CHECK_NEAR(proc.consumed_ml(), 100.0, 0.01);
}

QZ_TEST(single_refill_above_threshold)
{
    auto p = test_params(); auto o = test_options();
    QzRefillProcessor proc(p, o);
    std::string out = proc.process(synth_print(p, 5, 26.0)); // 130 ml
    QZ_CHECK(proc.events().size() == 1);
    QZ_CHECK(!proc.failed());
    QZ_CHECK(count_occurrences(out, "; QZ_REFILL_BEGIN") == 1);
    QZ_CHECK(count_occurrences(out, "; QZ_REFILL_END") == 1);
    QZ_CHECK(count_occurrences(out, "; QZ_REFILL_INDEX=1") == 1);
    // volume at the event must be >= threshold and <= usable capacity
    QZ_CHECK(proc.events()[0].volume_ml >= o.refill_threshold_ml);
    QZ_CHECK(proc.events()[0].volume_ml <= o.usable_capacity_ml);
}

QZ_TEST(multiple_refills_large_model)
{
    auto p = test_params(); auto o = test_options();
    QzRefillProcessor proc(p, o);
    std::string out = proc.process(synth_print(p, 10, 26.0)); // 260 ml -> 2 refills
    QZ_CHECK(proc.events().size() == 2);
    QZ_CHECK(count_occurrences(out, "; QZ_REFILL_BEGIN") == 2);
    QZ_CHECK(count_occurrences(out, "; QZ_REFILL_INDEX=2") == 1);
    QZ_CHECK(!proc.failed());
}

QZ_TEST(sequence_content_marlin)
{
    auto p = test_params(); auto o = test_options();
    o.prime_after_refill = true; o.prime_ml = 1.0;
    QzRefillProcessor proc(p, o);
    std::string out = proc.process(synth_print(p, 5, 26.0));
    QZ_CHECK(proc.events().size() == 1);
    size_t beg = out.find("; QZ_REFILL_BEGIN");
    size_t end = out.find("; QZ_REFILL_END");
    QZ_CHECK(beg != std::string::npos && end != std::string::npos && end > beg);
    std::string seq = out.substr(beg, end - beg);
    // preview marker tag present
    QZ_CHECK(seq.find("; PAUSE_PRINTING") != std::string::npos);
    // M400 before parking
    QZ_CHECK(seq.find("M400") != std::string::npos && seq.find("M400") < seq.find("G0 X5.000 Y5.000"));
    // relative E before plunger reset, reset before pause
    size_t m83 = seq.find("M83");
    size_t reset = seq.find("G1 E-");
    size_t pause = seq.find("M0 ;");
    QZ_CHECK(m83 != std::string::npos && reset != std::string::npos && pause != std::string::npos);
    QZ_CHECK(m83 < reset && reset < pause);
    // prime after pause
    size_t prime = seq.find("G1 E3.75", pause); // 1 ml ≈ 3.752 E
    QZ_CHECK(prime != std::string::npos);
    // logical E restored via G92 (never a physical +E return of the old depth)
    QZ_CHECK(seq.find("G92 E") != std::string::npos);
    // the plunger reset E amount must NOT be re-extruded after the pause
    const std::string reset_amount = seq.substr(reset + 5, seq.find(' ', reset + 5) - reset - 5);
    QZ_CHECK(seq.find("G1 E" + reset_amount, pause) == std::string::npos);
    // return: XY first, then Z down
    size_t ret_xy = seq.find("G0 X", pause);
    size_t ret_z  = seq.find("; lower to printing height", pause);
    QZ_CHECK(ret_xy != std::string::npos && ret_z != std::string::npos && ret_xy < ret_z);
}

QZ_TEST(modes_and_feedrate_preserved_across_refill)
{
    auto p = test_params(); auto o = test_options();
    QzRefillProcessor proc(p, o);
    std::string out = proc.process(synth_print(p, 5, 26.0));
    // After the sequence the state machine must still be in M83/G90 and the print
    // continues; feed the transformed output through a fresh state machine to verify
    // consistency of logical E accounting.
    QzGcodeStateMachine verify;
    std::istringstream is(out);
    std::string line;
    while (std::getline(is, line)) verify.process_line(line);
    QZ_CHECK(!verify.state().e_absolute);   // print body used M83
    QZ_CHECK(verify.state().xyz_absolute);  // G90 print
    // feedrate restore line exists
    QZ_CHECK(out.find("; restore feedrate") != std::string::npos);
}

QZ_TEST(absolute_e_mode_refill_restores_logical_e)
{
    auto p = test_params(); auto o = test_options();
    std::ostringstream g;
    g << "G90\nM82\nG28\nG92 E0\n";
    const double e_total = p.e_from_ml(125.0);
    const int segs = 10;
    double e = 0;
    for (int i = 0; i < segs; ++i) {
        g << "; CHANGE_LAYER\n";
        e += e_total / segs;
        g << "G1 X" << (10 + i) << " Y100 E" << e << " F1200\n";
        g << "G0 X" << (10 + i) << " Y10\n";
    }
    QzRefillProcessor proc(p, o);
    std::string out = proc.process(g.str());
    QZ_CHECK(proc.events().size() == 1);
    // G92 must restore the pre-refill logical E, and M82 must be restored
    size_t beg = out.find("; QZ_REFILL_BEGIN");
    std::string seq = out.substr(beg, out.find("; QZ_REFILL_END") - beg);
    QZ_CHECK(seq.find("M82 ; restore absolute E mode") != std::string::npos);
    QZ_CHECK(seq.find("G92 E") != std::string::npos);
}

QZ_TEST(threshold_crossing_near_layer_boundary)
{
    auto p = test_params(); auto o = test_options();
    // Layer volume chosen so the threshold is crossed by the LAST segment of a
    // layer: the refill must appear right after it (at the layer-change or the
    // following travel), within the reserve.
    QzRefillProcessor proc(p, o);
    std::string out = proc.process(synth_print(p, 6, 24.0)); // crosses 120 at end of layer 5
    QZ_CHECK(proc.events().size() == 1);
    QZ_CHECK(proc.events()[0].volume_ml <= o.usable_capacity_ml);
    QZ_CHECK(!proc.failed());
}

QZ_TEST(failure_when_no_safe_pause_before_capacity)
{
    auto p = test_params(); auto o = test_options();
    // One giant uninterrupted extrusion: threshold and usable capacity are both
    // crossed inside a single segment with no travel/layer boundary.
    std::ostringstream g;
    g << "G90\nM83\nG28\nG92 E0\n; CHANGE_LAYER\n";
    const double e_big = p.e_from_ml(140.0); // > usable 134.7
    g << "G1 X10 Y10 F3000\n"; // travel before, fine
    for (int i = 0; i < 140; ++i)
        g << "G1 X" << (10 + i) << " Y100 E" << (e_big / 140.0) << " F1200\n"; // continuous extrusion run
    QzRefillProcessor proc(p, o);
    std::string out = proc.process(g.str());
    QZ_CHECK(proc.failed());
    QZ_CHECK(!proc.error().empty());
    QZ_CHECK(proc.error().find("usable") != std::string::npos);
}

QZ_TEST(prime_included_in_next_cycle_budget)
{
    auto p = test_params(); auto o = test_options();
    o.prime_after_refill = true; o.prime_ml = 5.0;
    QzRefillProcessor proc(p, o);
    // 2 cycles: after refill 1, the new cycle starts with 5 ml already consumed.
    std::string out = proc.process(synth_print(p, 10, 25.0)); // 250 ml
    QZ_CHECK(proc.events().size() >= 2);
    // second event triggers earlier due to prime: consumed volume of cycle 2 counts the prime
    QZ_CHECK(proc.events()[1].volume_ml >= o.refill_threshold_ml);
}

QZ_TEST(firmware_adapters_pause_commands)
{
    QZ_CHECK(qz_pause_command(QzFirmwareFamily::Marlin, "auto", "", "").find("M0") == 0);
    QZ_CHECK(qz_pause_command(QzFirmwareFamily::Marlin, "auto", "M25 ; machine profile", "") == "M25 ; machine profile");
    QZ_CHECK(qz_pause_command(QzFirmwareFamily::Klipper, "auto", "", "").find("PAUSE") == 0);
    QZ_CHECK(qz_pause_command(QzFirmwareFamily::RepRapFirmware, "auto", "", "").find("M226") == 0);
    QZ_CHECK(qz_pause_command(QzFirmwareFamily::PrusaBuddy, "auto", "", "").find("M601") == 0);
    QZ_CHECK(qz_pause_command(QzFirmwareFamily::BambuLab, "auto", "", "").find("M400 U1") == 0);
    QZ_CHECK(qz_pause_command(QzFirmwareFamily::Marlin, "custom", "", "MY_MACRO") == "MY_MACRO");
    QZ_CHECK(qz_pause_command(QzFirmwareFamily::Marlin, "M600", "", "").find("M600") == 0);
    QZ_CHECK(qz_family_from_flavor("klipper") == QzFirmwareFamily::Klipper);
    QZ_CHECK(qz_family_from_flavor("marlin2") == QzFirmwareFamily::Marlin);
    QZ_CHECK(qz_family_from_flavor("reprapfirmware") == QzFirmwareFamily::RepRapFirmware);
}

QZ_TEST(volume_conservation_through_transform)
{
    auto p = test_params(); auto o = test_options();
    QzRefillProcessor proc(p, o);
    const double target_ml = 130.0;
    std::string in = synth_print(p, 5, 26.0);
    std::string out = proc.process(in);
    // consumed volume tracked by the processor matches the synthetic input within tolerance
    QZ_CHECK_NEAR(proc.consumed_ml(), target_ml, 0.5);
}
