// Quasizero Slicer - QzStackSim tests. GNU AGPLv3.
#include "standalone/qz_test.hpp"
#include "libslic3r/QuasiZero/QzStackSim.hpp"
#include <algorithm>
#include <cmath>

using namespace Slic3r::QuasiZero;

namespace {
struct Job {
    QzPasteMaterial mat;
    std::vector<QzLayerRecord> L;
    std::vector<QzLayerGeom>   G;
    std::vector<QzSimSegment>  segs;
    QzStabilityResult          res;
    std::vector<double>        lam;
};

// double-wall ring (R = 28 and 32 mm, 4 mm beads, 3 mm layers), optional half rings from layer `half_from`
Job make_ring_job(int n_layers, double t_layer, int half_from = -1, double cx = 100.0, double cy = 100.0)
{
    Job j;
    j.mat.rho = 1500.0; j.mat.tau0 = 550.0; j.mat.athix = 0.04; j.mat.E0 = 30000.0;
    double t = 0.0;
    const int nseg = 64;
    for (int k = 0; k < n_layers; ++k) {
        QzLayerRecord r;
        r.z_bottom = k * 3e-3; r.height = 3e-3; r.thickness = 4e-3; r.t_start = t; t += t_layer; r.t_end = t;
        j.L.push_back(r);
        QzLayerGeom g;
        g.cx = cx * 1e-3; g.cy = cy * 1e-3; g.area = 2 * 3.14159265 * 0.030 * 0.008; g.I_min = 3.14159265 * 0.030 * 0.030 * 0.030 * 0.008;
        g.dir_x = 1; g.dir_y = 0; g.r_max = 0.034;
        j.G.push_back(g);
        const int nuse = (half_from >= 0 && k >= half_from) ? nseg / 2 : nseg;
        for (double R : { 28.0, 32.0 })
            for (int s = 0; s < nuse; ++s) {
                const double a0 = 2 * 3.14159265 * s / nseg, a1 = 2 * 3.14159265 * (s + 1) / nseg;
                QzSimSegment sg;
                sg.x0 = (float) (cx + R * std::cos(a0)); sg.y0 = (float) (cy + R * std::sin(a0));
                sg.x1 = (float) (cx + R * std::cos(a1)); sg.y1 = (float) (cy + R * std::sin(a1));
                sg.z = (float) ((k + 1) * 3.0); sg.w = 4.0f; sg.h = 3.0f; sg.layer = k;
                sg.t_end = (float) (r.t_start + t_layer * (double) (s + 1) / (double) nuse);
                j.segs.push_back(sg);
            }
    }
    j.res = qz_evaluate_stability(j.mat, j.L, QzStabilityOptions{});
    j.lam = qz_buckling_load_factors(j.mat, j.L, j.G);
    return j;
}
}

QZ_TEST(stacksim_builds_and_matches_layer_model_before_collapse)
{
    Job j = make_ring_job(60, 125.0);
    QzStackSim sim;
    QZ_CHECK(sim.build(j.mat, j.L, j.G, j.segs, j.res, j.lam, QzStabilityOptions{}, QzSimOptions{}));
    QZ_CHECK(sim.valid() && sim.segments_count() == j.segs.size());
    QZ_CHECK(sim.cells_x() > 20 && sim.cells_y() > 20);
    QZ_CHECK(sim.collapse_step() == j.res.collapse_after_layer);
    QzSimFrame fr;
    const int top = 20;
    sim.frame(top, j.L[top].t_end, fr);
    QZ_CHECK(fr.valid && !fr.collapsed);
    // a segment of layer 0 sees the same load/strength as the layer model (uniform ring)
    const float u0 = sim.util(fr, 0);
    QZ_CHECK_NEAR(u0, j.res.history[top][0], 0.05 * std::max(0.1, (double) j.res.history[top][0]));
    // the top layer carries nothing
    QZ_CHECK(sim.util(fr, (int) j.segs.size() - 1) < 1e-6);
    // the frame's instantaneous maximum is the layer model's maximum at this step
    float mu = 0.0f;
    for (float v : j.res.history[top]) mu = std::max(mu, v);
    QZ_CHECK(fr.max_util > 0.05);
    QZ_CHECK_NEAR(fr.max_util, mu, 0.05 * mu);
    // the reported height is the real (settled) top, never above nominal
    QZ_CHECK(fr.height_deformed <= (top + 1) * 3.0 + 1e-4 && fr.height_deformed > (top + 1) * 3.0 * 0.8);
}

QZ_TEST(stacksim_bulge_band_above_base_and_settlement)
{
    Job j = make_ring_job(60, 125.0);
    QzStackSim sim; sim.build(j.mat, j.L, j.G, j.segs, j.res, j.lam, QzStabilityOptions{}, QzSimOptions{});
    const int kc = j.res.collapse_after_layer;
    QZ_CHECK(kc > 5);
    QzSimFrame fr;
    sim.frame(kc - 1, j.L[kc - 1].t_end, fr);
    QZ_CHECK(!fr.collapsed);
    // find the layer with the largest squash at the ring
    const int c = 0; (void) c;
    float best = 0.0f; int jb = -1;
    for (int layer = 0; layer < kc; ++layer) {
        const int si = (int) (layer * 128);   // first segment of the layer (128 segs per full layer)
        QzSimPoint a, b; float ws, hs; bool fallen;
        sim.deform(fr, si, a, b, ws, hs, fallen);
        QZ_CHECK(!fallen);
        if (ws - 1.0f > best) { best = ws - 1.0f; jb = layer; }
    }
    QZ_CHECK(best > 0.05f);
    QZ_CHECK(jb > 0 && jb < kc / 2);     // band above the bed
    // the top layer sits lower than nominal (settlement)
    QzSimPoint a, b; float ws, hs; bool fallen;
    sim.deform(fr, (kc - 1) * 128, a, b, ws, hs, fallen);
    QZ_CHECK(a.z < (float) (kc * 3.0) - 0.5f);
    QZ_CHECK(fr.height_deformed < kc * 3.0 - 0.5);
    // early frame: no deformation at all
    QzSimFrame f0; sim.frame(3, j.L[3].t_end, f0);
    sim.deform(f0, 0, a, b, ws, hs, fallen);
    QZ_CHECK_NEAR(ws, 1.0, 1e-6); QZ_CHECK_NEAR(a.z, 3.0, 1e-4);
}

QZ_TEST(stacksim_within_layer_variation_for_irregular_geometry)
{
    // from layer 20 on, only half of the ring is printed: the base under the tall half carries more
    Job j = make_ring_job(45, 125.0, 20);
    QzStackSim sim; sim.build(j.mat, j.L, j.G, j.segs, j.res, j.lam, QzStabilityOptions{}, QzSimOptions{});
    QzSimFrame fr;
    const int top = 40;
    sim.frame(top, j.L[top].t_end, fr);
    // layer 0: segment 0 (angle 0, printed half) vs segment 40 (angle ~225 deg, unprinted half)
    const float u_tall = sim.util(fr, 0), u_low = sim.util(fr, 40);
    QZ_CHECK(u_tall > 1.5f * u_low);
}

QZ_TEST(stacksim_after_collapse_strands_fall_on_the_pile)
{
    Job j = make_ring_job(60, 125.0);
    QzStackSim sim; sim.build(j.mat, j.L, j.G, j.segs, j.res, j.lam, QzStabilityOptions{}, QzSimOptions{});
    const int kc = sim.collapse_step();
    QZ_CHECK(kc > 0);
    QzSimFrame fr;
    const int top = kc + 6;
    sim.frame(top, j.L[top].t_end + 500.0, fr);
    QZ_CHECK(fr.collapsed && fr.fold_angle > 1.0 && fr.hinge_layer == j.res.critical_layer);
    // pre-collapse layer above the hinge: rotated towards +x, lower than nominal
    QzSimPoint a, b; float ws, hs; bool fallen;
    sim.deform(fr, (kc - 1) * 128, a, b, ws, hs, fallen);
    QZ_CHECK(!fallen);
    QZ_CHECK(a.z < (float) (kc * 3.0));
    // strands of a layer deposited after the collapse are fallen: nominal XY (+ jitter <= 1 bead), z on the pile
    const int si = (kc + 3) * 128 + 10;
    sim.deform(fr, si, a, b, ws, hs, fallen);
    QZ_CHECK(fallen);
    const QzSimSegment &s = sim.segment(si);
    QZ_CHECK(std::fabs(a.x - s.x0) <= 0.5f * s.w + 1e-4f && std::fabs(a.y - s.y0) <= 0.5f * s.w + 1e-4f);
    QZ_CHECK(a.z > 0.0f && a.z < s.z);            // landed below its nominal height
    QZ_CHECK(ws > 1.0f && hs < 1.0f);             // flattened
    // later strands pile higher on average than earlier ones on the same spot
    QzSimPoint a2, b2;
    sim.deform(fr, (kc + 5) * 128 + 10, a2, b2, ws, hs, fallen);
    QZ_CHECK(fallen && a2.z >= a.z - 1e-4f);
    // determinism
    QzSimFrame fr2; sim.frame(top, j.L[top].t_end + 500.0, fr2);
    QzSimPoint a3, b3; sim.deform(fr2, si, a3, b3, ws, hs, fallen);
    QZ_CHECK_NEAR(a3.x, a.x, 1e-6); QZ_CHECK_NEAR(a3.z, a.z, 1e-6);
}

QZ_TEST(stacksim_ratio_remembers_peak_and_matches_layer_field)
{
    Job j = make_ring_job(60, 125.0);
    QzStackSim sim; sim.build(j.mat, j.L, j.G, j.segs, j.res, j.lam, QzStabilityOptions{}, QzSimOptions{});
    QzSimFrame fr;
    float last = 0.0f;
    for (int top = 0; top < 50; ++top) {
        sim.frame(top, j.L[top].t_end, fr);
        const float r0 = sim.ratio(fr, 0);                    // a strand of layer 0
        QZ_CHECK(r0 >= sim.util(fr, 0) - 1e-6f);              // never below the instantaneous value
        QZ_CHECK(r0 >= last - 1e-6f);                         // never resets as the print moves on
        QZ_CHECK(fr.max_ratio >= fr.max_util - 1e-6);
        // uniform ring: the per-cell field equals the per-layer field with memory
        const std::vector<float> f = qz_utilization_upto(j.res, top);
        QZ_CHECK_NEAR(r0, f[0], 0.05 * std::max(0.05, (double) f[0]));
        last = r0;
    }
    QZ_CHECK(last > 0.5f);
}
