// Quasizero Slicer - the deformation view's play clock: sub-vertex interpolation while
// playing, the layer-change transient, and a layers slider dragged during the play (which
// used to freeze the deformed mesh until pause/play). GNU AGPLv3.
#include "standalone/qz_test.hpp"
#include "slic3r/Utils/QzPlayClock.hpp"

using Slic3r::GUI::QzPlayClock;

QZ_TEST(play_clock_interpolates_between_vertices_only_while_playing)
{
    QzPlayClock c;
    c.fraction = 0.5;
    QZ_CHECK_NEAR(c.instant(100.0, 104.0), 100.0, 1e-9);   // paused: the vertex's own time
    c.on_frame(true);
    QZ_CHECK_NEAR(c.instant(100.0, 104.0), 102.0, 1e-9);   // playing: half way to the next vertex
    c.fraction = 1.7;                                        // never past the next vertex
    QZ_CHECK_NEAR(c.instant(100.0, 104.0), 104.0, 1e-9);
    QZ_CHECK_NEAR(c.instant(100.0, 100.0), 100.0, 1e-9);   // last vertex: nothing to interpolate to
}

QZ_TEST(play_clock_rebuilds_on_vertex_or_time_change_with_a_finer_step_in_slow_motion)
{
    QzPlayClock c;
    QZ_CHECK(c.needs_rebuild(10, 5.0, false));               // nothing cached yet
    c.mark(10, 5.0);
    QZ_CHECK(!c.needs_rebuild(10, 5.3, false));              // same vertex, 0.3 s: not worth it at normal speed
    QZ_CHECK(c.needs_rebuild(10, 5.3, true));                // ... but it is in slow motion
    QZ_CHECK(c.needs_rebuild(11, 5.3, false));               // a new vertex always is
    QZ_CHECK(c.needs_rebuild(10, 5.6, false));               // half a second of print time
}

QZ_TEST(play_clock_skips_the_layer_change_transient_but_follows_a_dragged_slider)
{
    QzPlayClock c;
    c.on_frame(true);
    c.mark(100, 60.0);
    // the play loop moved to the next layer: this frame the range ends at the END of that
    // layer (far ahead) - not drawn
    c.layer_transient = true;
    QZ_CHECK(!c.needs_rebuild(400, 130.0, false));
    // next frame: the sliders may still show the end of the layer for a frame or two
    c.on_frame(true);
    QZ_CHECK(!c.needs_rebuild(400, 130.0, false));
    c.on_frame(true);
    QZ_CHECK(!c.needs_rebuild(401, 130.2, false));
    // ... then the moves slider is back at the start of the new layer: a small step, drawn
    c.on_frame(true);
    QZ_CHECK(c.needs_rebuild(101, 60.4, false));
    c.mark(101, 60.4);

    // the user drags the layers slider far up while the player runs: the range stays ahead
    // frame after frame - after two frames it is a real position and must be drawn
    c.on_frame(true);
    QZ_CHECK(!c.needs_rebuild(900, 300.0, false));
    c.on_frame(true);
    QZ_CHECK(!c.needs_rebuild(901, 300.3, false));
    c.on_frame(true);
    QZ_CHECK(c.needs_rebuild(902, 300.6, false));
    c.mark(902, 300.6);
    c.on_frame(true);
    QZ_CHECK(c.needs_rebuild(903, 301.2, false));            // and it keeps following from there

    // paused: a jump is a jump, drawn at once
    c.on_frame(false);
    QZ_CHECK(c.needs_rebuild(50, 20.0, false));
}
