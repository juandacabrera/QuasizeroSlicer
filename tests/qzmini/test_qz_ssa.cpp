// Quasizero Slicer - Short-Segment Anchoring tests. GNU AGPLv3.
#include "standalone/qz_test.hpp"
#include "libslic3r/QuasiZero/QzShortSegmentAnchor.hpp"

using namespace Slic3r::QuasiZero;

namespace {
QzSsaOptions opts(bool e_rel)
{
    QzSsaOptions o;
    o.max_length_mm = 2.0;
    o.dwell_ms = 250;
    o.extra_prime_e = 0.4;
    o.depart_speed_mms = 10.0;
    o.initial_e_relative = e_rel;
    return o;
}
size_t count_of(const std::string &s, const std::string &needle)
{
    size_t n = 0, pos = 0;
    while ((pos = s.find(needle, pos)) != std::string::npos) { ++n; pos += needle.size(); }
    return n;
}
} // namespace

QZ_TEST(ssa_short_island_gets_prime_dwell_and_slow_depart)
{
    QzShortSegmentAnchor ssa(opts(true));
    const std::string in =
        "G1 X0 Y0 F3600\n"
        "G1 X1 Y0 E0.5 F420\n"       // 1 mm island (short)
        "G1 X30 Y0 F3600\n"          // travel out
        "G1 X60 Y0 E12.0 F420\n";    // 30 mm island (long)
    const std::string out = ssa.process(in);
    QZ_CHECK(count_of(out, "QZ_SSA_PRIME") == 1);
    QZ_CHECK(count_of(out, "G4 P250 ; QZ_SSA_DWELL") == 1);
    QZ_CHECK(count_of(out, "QZ_SSA_DEPART") == 1);
    QZ_CHECK(count_of(out, "F600") == 1);              // 10 mm/s cap on the depart travel
    QZ_CHECK(count_of(out, "QZ_SSA_RESTORE_F") >= 1);  // modal F comes back
    QZ_CHECK(ssa.anchored_islands() == 1);
    // prime is relative in M83 mode
    QZ_CHECK(out.find("G1 E0.40000 F300 ; QZ_SSA_PRIME") != std::string::npos);
    // the dwell sits after the island, before the travel
    QZ_CHECK(out.find("QZ_SSA_DWELL") < out.find("QZ_SSA_DEPART"));
}

QZ_TEST(ssa_absolute_mode_preserves_logical_e)
{
    QzShortSegmentAnchor ssa(opts(false));
    const std::string in =
        "M82\n"
        "G92 E0\n"
        "G1 X0 Y0 F3600\n"
        "G1 X1 Y0 E0.5 F420\n"
        "G1 X30 Y0 F3600\n";
    const std::string out = ssa.process(in);
    // physical prime then logical reset to the island start (E0)
    QZ_CHECK(out.find("G1 E0.40000 F300 ; QZ_SSA_PRIME") != std::string::npos);
    QZ_CHECK(out.find("G92 E0.00000 ; QZ_SSA_PRIME_RESET") != std::string::npos);
    QZ_CHECK(ssa.anchored_islands() == 1);
}

QZ_TEST(ssa_long_islands_pass_untouched)
{
    QzShortSegmentAnchor ssa(opts(true));
    const std::string in =
        "G1 X0 Y0 F3600\n"
        "G1 X50 Y0 E10.0 F420\n"
        "G1 X100 Y0 E10.0 F420\n"
        "G1 X0 Y50 F3600\n";
    const std::string out = ssa.process(in);
    QZ_CHECK(count_of(out, "QZ_SSA") == 0);
    QZ_CHECK(ssa.anchored_islands() == 0);
    QZ_CHECK(out.find("G1 X50 Y0 E10.0 F420") != std::string::npos);
}

QZ_TEST(ssa_disabled_prime_still_dwells)
{
    QzSsaOptions o = opts(true);
    o.extra_prime_e = 0.0;
    QzShortSegmentAnchor ssa(o);
    const std::string in =
        "G1 X0 Y0 F3600\n"
        "G1 X1 Y0 E0.5 F420\n"
        "G1 X30 Y0 F3600\n";
    const std::string out = ssa.process(in);
    QZ_CHECK(count_of(out, "QZ_SSA_PRIME") == 0);
    QZ_CHECK(count_of(out, "QZ_SSA_DWELL") == 1);
}
