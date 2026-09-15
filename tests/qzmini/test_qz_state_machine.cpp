// Quasizero Slicer — unit tests for the G-code state machine. GNU AGPLv3.
#include "standalone/qz_test.hpp"
#include "libslic3r/QuasiZero/QzGcodeStateMachine.hpp"

using namespace Slic3r::QuasiZero;

QZ_TEST(relative_extrusion_m83)
{
    QzGcodeStateMachine sm;
    sm.process_line("M83");
    sm.process_line("G1 X10 Y0 E2.5 F1200");
    sm.process_line("G1 X20 Y0 E2.5");
    QZ_CHECK_NEAR(sm.state().deposited_e_total, 5.0, 1e-12);
    QZ_CHECK_NEAR(sm.state().logical_e, 5.0, 1e-12);
    QZ_CHECK(!sm.state().e_absolute);
    QZ_CHECK_NEAR(sm.state().feedrate, 1200.0, 1e-12);
}

QZ_TEST(absolute_extrusion_m82)
{
    QzGcodeStateMachine sm;
    sm.process_line("M82");
    sm.process_line("G92 E0");
    sm.process_line("G1 X10 E4.0");
    sm.process_line("G1 X20 E9.0");
    QZ_CHECK_NEAR(sm.state().deposited_e_total, 9.0, 1e-12);
    QZ_CHECK_NEAR(sm.state().logical_e, 9.0, 1e-12);
}

QZ_TEST(g92_e_reset_no_physical_motion)
{
    QzGcodeStateMachine sm;
    sm.process_line("M82");
    sm.process_line("G1 E10");
    sm.process_line("G92 E0");     // logical reset only
    sm.process_line("G1 E5");      // 5 more physical units
    QZ_CHECK_NEAR(sm.state().deposited_e_total, 15.0, 1e-12);
    QZ_CHECK_NEAR(sm.state().logical_e, 5.0, 1e-12);
}

QZ_TEST(retract_unretract_pair_not_counted)
{
    QzGcodeStateMachine sm;
    sm.process_line("M83");
    sm.process_line("G1 X10 E5");
    sm.process_line("G1 E-2");         // retract
    sm.process_line("G0 X50");         // travel
    sm.process_line("G1 E2");          // unretract exactly
    QZ_CHECK_NEAR(sm.state().deposited_e_total, 5.0, 1e-12);
    sm.process_line("G1 X60 E1");
    QZ_CHECK_NEAR(sm.state().deposited_e_total, 6.0, 1e-12);
    QZ_CHECK_NEAR(sm.state().pending_retract_e, 0.0, 1e-12);
}

QZ_TEST(partial_unretract)
{
    QzGcodeStateMachine sm;
    sm.process_line("M83");
    sm.process_line("G1 E-3");
    sm.process_line("G1 E5"); // 3 unretract + 2 deposit
    QZ_CHECK_NEAR(sm.state().deposited_e_total, 2.0, 1e-12);
}

QZ_TEST(xyz_modes_and_positions)
{
    QzGcodeStateMachine sm;
    sm.process_line("G90");
    sm.process_line("G1 X10 Y20 Z3");
    sm.process_line("G91");
    sm.process_line("G1 X5 Y-5 Z1");
    QZ_CHECK_NEAR(sm.state().x, 15.0, 1e-12);
    QZ_CHECK_NEAR(sm.state().y, 15.0, 1e-12);
    QZ_CHECK_NEAR(sm.state().z, 4.0, 1e-12);
    QZ_CHECK(!sm.state().xyz_absolute);
    sm.process_line("G90");
    QZ_CHECK(sm.state().xyz_absolute);
    QZ_CHECK(sm.state().e_absolute); // G90 restores absolute E (Marlin semantics)
}

QZ_TEST(g28_home)
{
    QzGcodeStateMachine sm;
    sm.process_line("G1 X100 Y100 Z50");
    sm.process_line("G28");
    QZ_CHECK_NEAR(sm.state().x, 0.0, 1e-12);
    QZ_CHECK_NEAR(sm.state().z, 0.0, 1e-12);
}

QZ_TEST(layer_change_comments_detected)
{
    QzGcodeStateMachine sm;
    auto a = sm.process_line("; CHANGE_LAYER");
    auto b = sm.process_line(";AFTER_LAYER_CHANGE");
    auto c = sm.process_line("; just a comment");
    QZ_CHECK(a.is_layer_change);
    QZ_CHECK(b.is_layer_change);
    QZ_CHECK(!c.is_layer_change);
    QZ_CHECK(sm.state().layer_count == 2);
}

QZ_TEST(travel_vs_extruding_and_arcs)
{
    QzGcodeStateMachine sm;
    sm.process_line("M83");
    auto t = sm.process_line("G0 X10 Y10");
    auto e = sm.process_line("G1 X20 E1");
    auto a = sm.process_line("G2 X30 Y10 I5 J0 E0.5");
    QZ_CHECK(t.is_travel);
    QZ_CHECK(!e.is_travel && e.is_extruding);
    QZ_CHECK(a.kind == QzLineInfo::Kind::ArcMove);
    QZ_CHECK_NEAR(sm.state().deposited_e_total, 1.5, 1e-12);
}

QZ_TEST(pause_commands_detected)
{
    QzGcodeStateMachine sm;
    QZ_CHECK(sm.process_line("M0").kind == QzLineInfo::Kind::Pause);
    QZ_CHECK(sm.process_line("M25").kind == QzLineInfo::Kind::Pause);
    QZ_CHECK(sm.process_line("M600").kind == QzLineInfo::Kind::Pause);
    QZ_CHECK(sm.process_line("M601").kind == QzLineInfo::Kind::Pause);
    QZ_CHECK(sm.process_line("M226").kind == QzLineInfo::Kind::Pause);
}

QZ_TEST(comments_and_metadata_ignored_for_motion)
{
    QzGcodeStateMachine sm;
    sm.process_line("; FEATURE: Outer wall");
    sm.process_line(";LAYER_HEIGHT: 3");
    sm.process_line("G1 X10 E1 ; inline comment E99 should not parse");
    QZ_CHECK_NEAR(sm.state().deposited_e_total, 1.0, 1e-12);
}
