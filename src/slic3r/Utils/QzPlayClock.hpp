// Quasizero Slicer - the moves player's clock as the deformation view sees it.
// GNU AGPLv3, part of the Quasizero fork of OrcaSlicer.
//
// The player advances whole vertices; the deformation view draws the print at an instant.
// Two things follow. (1) While playing, the instant is interpolated between the last
// visible vertex and the next one with the play loop's fractional progress: around the
// predicted collapse the player runs at about a third of real speed and consecutive
// vertices can be seconds of print time apart, so whole-vertex updates would play the fold
// in visible steps. (2) A layer change in the play leaves the visible range at the END of
// the new layer for a frame or two, until the moves slider is reset: that transient must not
// be drawn - but a range that STAYS ahead (the user dragging the layers slider while the
// player runs) is a real position and must be.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace Slic3r {
namespace GUI {

struct QzPlayClock
{
    bool   playing         = false;
    bool   layer_transient = false;     // the play loop moved to the next layer this frame
    double fraction        = 0.0;       // progress towards the next vertex, from the play loop
    size_t cache_vertex    = size_t(-1);
    double cache_time      = -1.0;
    int    jump_frames     = 0;         // consecutive frames with the range far ahead of the cache

    void reset() { *this = QzPlayClock(); }
    void on_frame(bool is_playing) { playing = is_playing; layer_transient = false; }
    void invalidate() { cache_vertex = size_t(-1); cache_time = -1.0; jump_frames = 0; }

    // the instant to draw for a visible range ending at a vertex of estimated time t_vertex
    // (t_next: the next vertex's, or t_vertex when there is none)
    double instant(double t_vertex, double t_next) const
    {
        if (!playing || t_next <= t_vertex) return t_vertex;
        return t_vertex + std::min(1.0, std::max(0.0, fraction)) * (t_next - t_vertex);
    }

    // whether the mesh must be rebuilt for (vertex, t). `fine`: small steps of time matter
    // (slow motion around the collapse), otherwise a rebuild every half second of print time
    bool needs_rebuild(size_t vertex, double t, bool fine)
    {
        bool transient = layer_transient;
        if (!transient && playing && cache_vertex != size_t(-1) && vertex > cache_vertex && t - cache_time > 10.0) {
            // a forward jump of more than a few seconds of print time while playing: the
            // one-or-two-frame layer-change transient, or the user moving the layers slider
            ++jump_frames;
            transient = jump_frames <= 2;
        } else
            jump_frames = 0;
        if (transient) return false;
        const double eps = fine ? 0.02 : 0.5;
        return vertex != cache_vertex || std::fabs(t - cache_time) > eps;
    }
    void mark(size_t vertex, double t) { cache_vertex = vertex; cache_time = t; }
};

} // namespace GUI
} // namespace Slic3r
