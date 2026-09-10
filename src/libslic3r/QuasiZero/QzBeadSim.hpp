// Quasizero Slicer - QzBeadSim: the bead that keeps coming out of the nozzle after the print
// has collapsed, as a chain of particles with position-based dynamics. GNU AGPLv3.
//
// IMG_4657 (last frames): the head follows its nominal path, the strand hangs from the
// nozzle, falls under gravity, lands on the fallen part or the bed and piles up strand after
// strand. Model (handoff spec section 3.4): particles emitted at the nozzle at the real
// cadence (one every bead width of path), Verlet integration with gravity and strong viscous
// damping, inextensible distance constraints along the chain, a weak bending smoothing of
// the hanging part, a little smooth lateral noise at emission, collision with a height field
// of everything already deposited (standing/folded stack + the strand itself once it comes to
// rest); the strand lying on the pile is plastic: it keeps the shape it fell into and takes
// only a small share of the chain corrections (friction). Deterministic for a
// given seed; snapshots make scrubbing backwards cheap. Pure C++17, testable standalone.
#pragma once

#include "QzStackSim.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Slic3r { namespace QuasiZero {

struct QzBeadOptions
{
    double dt            = 1.0 / 120.0; // [s] integration step (print time)
    double gravity       = 9810.0;     // [mm/s^2]
    double damping       = 0.85;       // velocity kept per step (viscous paste)          [hyp]
    double spacing_beads = 1.0;        // particle spacing in bead widths
    int    iterations    = 8;          // constraint passes per step (two sweeps each; a hanging strand stretches with fewer)
    double bend_stiff    = 0.1;        // bending smoothing of the hanging strand (0..1); the laid strand is plastic [hyp]
    double laid_share    = 0.2;        // share of a link correction taken by a particle lying on the pile (friction/yield) [hyp]
    double noise_amp     = 0.2;        // lateral emission noise, fraction of the nozzle speed [hyp]
    double rest_time     = 0.5;        // [s] a particle at rest this long is frozen and stamped into the field
    double flatten       = 1.35;       // width factor of the bead once at rest (height 1/this)
    double wall_beads    = 1.5;        // a field step taller than this many bead heights is a wall (stops the strand, no lift)
    size_t max_particles = 20000;      // spacing grows when the print would exceed this
    double max_duration  = 900.0;      // [s] print time simulated after the collapse; later the strand just rests
    double snapshot_dt   = 20.0;       // [s] state snapshots for scrubbing backwards
    uint32_t seed        = 0x2545F491u;
};

struct QzBeadParticle
{
    float x = 0, y = 0, z = 0;        // [mm] bead centre
    float px = 0, py = 0, pz = 0;     // previous position (Verlet)
    float t_emit = 0;                 // [s] emission time
    float rest_since = -1.0f;         // [s] time it came to rest (< 0 = moving)
    int   chain = 0;                  // chain id (a travel starts a new chain)
    bool  frozen = false;             // stamped into the field, no longer simulated
};

class QzBeadSim
{
public:
    // Segments after the collapse (nozzle path, in deposition order, times = QzSimSegment::t_end),
    // t_start = time the first segment starts, the settled height field of the stack and the
    // grid it lives on (same as the stack simulator). w/h = bead section.
    bool build(const std::vector<QzSimSegment> &path, double t_start, const std::vector<float> &land,
               double x0, double y0, double cell, int nx, int ny, const QzBeadOptions &opt);
    bool valid() const { return m_valid; }

    // advance (or rewind, through the snapshots) to print time t
    void seek(double t);
    double time() const { return m_time; }
    const std::vector<QzBeadParticle> &particles() const { return m_p; }
    // nozzle position at the current time (the strand hangs from it) - false before the path starts
    bool nozzle(float &x, float &y, float &z) const;
    // polylines to draw: chains of particle indices, the last one ends at the nozzle
    const std::vector<std::vector<int>> &chains() const { return m_chains; }
    int    active_chain() const { return m_active_chain; }
    float  bead_w() const { return m_w; }
    float  bead_h() const { return m_h; }
    double spacing() const { return m_spacing; }
    const std::vector<float> &field() const { return m_land; }

private:
    struct Snapshot { double time; std::vector<QzBeadParticle> p; std::vector<float> land; size_t next_seg; double seg_s; bool seg_fresh; std::vector<std::vector<int>> chains; int active_chain; };
    void   reset();
    void   step();
    void   emit_until(double t);
    float  field_at(float x, float y) const;
    void   stamp(const QzBeadParticle &q, const QzBeadParticle *prev, const QzBeadParticle *next);
    float  noise(float t, uint32_t salt) const;
    void   nozzle_at(double t, float &x, float &y, float &z, float &vx, float &vy, bool &extruding) const;

    bool m_valid = false;
    QzBeadOptions m_opt;
    std::vector<QzSimSegment> m_path;   // nozzle path
    std::vector<float> m_t0;            // start time of each path segment
    double m_t_start = 0.0, m_t_end = 0.0;
    float  m_w = 4.0f, m_h = 3.0f;
    double m_spacing = 4.0;
    // grid
    std::vector<float> m_land0, m_land;
    double m_x0 = 0, m_y0 = 0, m_cell = 2.0; int m_nx = 0, m_ny = 0;
    // state
    double m_time = 0.0;
    std::vector<QzBeadParticle> m_p;
    std::vector<std::vector<int>> m_chains;
    int    m_active_chain = -1;
    size_t m_next_seg = 0;             // emission cursor: segment index
    double m_seg_s = 0.0;              // path length already emitted inside m_next_seg
    bool   m_seg_fresh = true;         // m_next_seg not entered yet (travel check pending)
    std::vector<Snapshot> m_snaps;
    std::vector<uint8_t>  m_touch;     // per particle, this step: lying on the field (scratch)
    std::vector<size_t>   m_live0;     // per chain: index of its first non-frozen particle (cache)
};

}} // namespace Slic3r::QuasiZero
