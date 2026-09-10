// Quasizero Slicer - QzBeadSim tests. GNU AGPLv3.
#include "standalone/qz_test.hpp"
#include "libslic3r/QuasiZero/QzBeadSim.hpp"
#include "libslic3r/QuasiZero/QzSkeleton.hpp"
#include <cmath>

using namespace Slic3r::QuasiZero;

namespace {
// nozzle path: n_layers rings at 100 mm and above (the print continues high over a collapsed
// stack), 3 mm/s, plus a flat field with a 10 mm high block on the +x half (a cliff: its side
// is a wall for the strand, the bed on the -x half is a lot further down)
struct Scene {
    std::vector<QzSimSegment> path;
    std::vector<float> land;
    double x0 = 40.0, y0 = 40.0, cell = 2.0; int nx = 60, ny = 60;
    double t_start = 1000.0;
};
Scene make_scene(int n_layers, double z_first = 100.0, int nseg = 48)
{
    Scene sc;
    double t = sc.t_start;
    const double R = 17.5, cx = 100.0, cy = 100.0, speed = 3.0;
    for (int k = 0; k < n_layers; ++k)
        for (int s = 0; s < nseg; ++s) {
            const double a0 = 2 * 3.14159265 * s / nseg, a1 = 2 * 3.14159265 * (s + 1) / nseg;
            QzSimSegment sg;
            sg.x0 = (float) (cx + R * std::cos(a0)); sg.y0 = (float) (cy + R * std::sin(a0));
            sg.x1 = (float) (cx + R * std::cos(a1)); sg.y1 = (float) (cy + R * std::sin(a1));
            sg.z = (float) (z_first + 3.0 * k); sg.w = 4.0f; sg.h = 3.0f; sg.layer = 30 + k;
            t += std::hypot(sg.x1 - sg.x0, sg.y1 - sg.y0) / speed; sg.t_end = (float) t;
            sc.path.push_back(sg);
        }
    sc.land.assign((size_t) sc.nx * sc.ny, 0.0f);
    for (int iy = 0; iy < sc.ny; ++iy)
        for (int ix = 0; ix < sc.nx; ++ix)
            if (sc.x0 + ix * sc.cell >= 100.0) sc.land[(size_t) iy * sc.nx + ix] = 10.0f;
    return sc;
}
} // namespace

QZ_TEST(beadsim_emits_at_the_nozzle_cadence_falls_and_rests_on_the_field)
{
    Scene sc = make_scene(3);
    QzBeadSim sim;
    QzBeadOptions opt;
    QZ_CHECK(sim.build(sc.path, sc.t_start, sc.land, sc.x0, sc.y0, sc.cell, sc.nx, sc.ny, opt));
    QZ_CHECK_NEAR(sim.spacing(), 4.0, 1e-9);
    // after 20 s at 3 mm/s the nozzle laid 60 mm of bead -> ~15 particles, all below the nozzle height
    sim.seek(sc.t_start + 20.0);
    const size_t n20 = sim.particles().size();
    QZ_CHECK(n20 >= 13 && n20 <= 17);
    float nx, ny, nz; QZ_CHECK(sim.nozzle(nx, ny, nz));
    QZ_CHECK_NEAR(nz, 100.0, 1e-4);
    bool any_low = false;
    for (const QzBeadParticle &q : sim.particles()) {
        QZ_CHECK(q.z <= 100.0f + 1e-3f);
        // never inside the field
        int ix = (int) std::floor((q.x - sc.x0) / sc.cell), iy = (int) std::floor((q.y - sc.y0) / sc.cell);
        ix = std::max(0, std::min(sc.nx - 1, ix)); iy = std::max(0, std::min(sc.ny - 1, iy));
        QZ_CHECK(q.z >= sc.land[(size_t) iy * sc.nx + ix] + 0.5f * 3.0f - 1e-3f);
        if (q.z < 60.0f) any_low = true;
    }
    QZ_CHECK(any_low);     // the older ones have fallen far from the nozzle
    // the chain hangs from the nozzle: its last particle is within one spacing (+ a little) of it
    const auto &ch = sim.chains();
    QZ_CHECK(!ch.empty() && sim.active_chain() >= 0);
    const QzBeadParticle &last = sim.particles()[(size_t) ch[(size_t) sim.active_chain()].back()];
    QZ_CHECK(std::hypot(std::hypot(last.x - nx, last.y - ny), last.z - nz) <= 1.5 * sim.spacing() + 1e-3);
    // inextensible: consecutive particles at most ~1.3 spacings apart (the solver is iterative)
    for (const std::vector<int> &c : ch)
        for (size_t k = 0; k + 1 < c.size(); ++k) {
            const QzBeadParticle &a = sim.particles()[(size_t) c[k]], &b = sim.particles()[(size_t) c[k + 1]];
            QZ_CHECK(std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z)) <= 1.3 * sim.spacing() + 1e-3);
        }
    // by the end of the path everything early has come to rest and was stamped into the field
    sim.seek(sc.path.back().t_end + 5.0);
    size_t frozen = 0; float fmax = 0.0f;
    for (const QzBeadParticle &q : sim.particles()) if (q.frozen) ++frozen;
    for (float v : sim.field()) fmax = std::max(fmax, v);
    QZ_CHECK(frozen > sim.particles().size() / 2);
    QZ_CHECK(fmax > 10.0f);                                    // the pile grew on the block
    // particles that came to rest on the block side sit on top of it (never inside: the cliff
    // is a wall); on the bed side the hanging loop sinks until it lies on the bed itself
    int on_block = 0, on_bed = 0, block_side = 0;
    for (const QzBeadParticle &q : sim.particles()) {
        if (q.x >= 104.0f) { ++block_side; QZ_CHECK(q.z >= 11.5f - 1e-3f); if (q.frozen) ++on_block; }
        else if (q.x <= 96.0f && q.z < 3.0f) ++on_bed;
    }
    QZ_CHECK(block_side > 10);
    QZ_CHECK(on_block > 5);
    QZ_CHECK(on_bed > 5);
    // the last chain still hangs from the head, which stopped at the end of the path
    QZ_CHECK(sim.nozzle(nx, ny, nz));
    const QzBeadParticle &tail = sim.particles()[(size_t) sim.chains()[(size_t) sim.active_chain()].back()];
    QZ_CHECK(std::hypot(std::hypot(tail.x - nx, tail.y - ny), tail.z - nz) <= 1.5 * sim.spacing() + 1e-3);
}

QZ_TEST(beadsim_is_deterministic_and_rewinds_through_snapshots)
{
    Scene sc = make_scene(3);
    QzBeadOptions opt; opt.snapshot_dt = 10.0;
    QzBeadSim a, b;
    QZ_CHECK(a.build(sc.path, sc.t_start, sc.land, sc.x0, sc.y0, sc.cell, sc.nx, sc.ny, opt));
    QZ_CHECK(b.build(sc.path, sc.t_start, sc.land, sc.x0, sc.y0, sc.cell, sc.nx, sc.ny, opt));
    const double t1 = sc.t_start + 35.0;
    a.seek(t1);
    b.seek(sc.path.back().t_end + 2.0);   // far ahead
    b.seek(t1);                           // and back through the snapshots
    QZ_CHECK(a.particles().size() == b.particles().size());
    QZ_CHECK_NEAR(a.time(), b.time(), 0.6 * opt.dt + 1e-6);
    double err = 0.0;
    for (size_t i = 0; i < a.particles().size(); ++i)
        err = std::max(err, (double) std::fabs(a.particles()[i].z - b.particles()[i].z) + std::fabs(a.particles()[i].x - b.particles()[i].x));
    QZ_CHECK(err < 1e-3);
    // and rendering: the chains mesh into one component each
    QzTubeOptions topt; topt.bins = 4;
    std::vector<QzTubeGeometry> geos(4);
    for (const std::vector<int> &c : a.chains()) {
        std::vector<QzSimPoint> pts;
        for (int idx : c) { const QzBeadParticle &q = a.particles()[(size_t) idx]; pts.push_back({ q.x, q.y, q.z }); }
        QzTubeMesher::mesh_polyline(pts, a.bead_w(), a.bead_h(), 0.05f, topt, geos);
    }
    size_t nv = 0; for (const QzTubeGeometry &g : geos) nv += g.vertices_count();
    QZ_CHECK(nv > 0);
}

QZ_TEST(beadsim_travel_starts_a_new_chain_and_particle_cap_holds)
{
    Scene sc = make_scene(2);
    // a jump in the middle of the path: move the second layer 40 mm away
    for (QzSimSegment &s : sc.path) if (s.layer == 31) { s.x0 += 40.0f; s.x1 += 40.0f; }
    QzBeadOptions opt; opt.max_particles = 200;
    QzBeadSim sim;
    QZ_CHECK(sim.build(sc.path, sc.t_start, sc.land, sc.x0, sc.y0, sc.cell, sc.nx, sc.ny, opt));
    sim.seek(sc.path.back().t_end + 1.0);
    QZ_CHECK(sim.chains().size() == 2);
    QZ_CHECK(sim.particles().size() <= 200);
}
