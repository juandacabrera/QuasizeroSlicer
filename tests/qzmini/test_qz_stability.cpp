// Quasizero Slicer - paste stability model tests. GNU AGPLv3.
#include "standalone/qz_test.hpp"
#include "libslic3r/QuasiZero/QzStabilityModel.hpp"
#include <cmath>

using namespace Slic3r::QuasiZero;

namespace {
// uniform schedule: n layers of height h [mm], t_layer seconds each, single bead width w [mm]
std::vector<QzLayerRecord> uniform_layers(int n, double h_mm, double t_layer, double w_mm, int pause_every = 0, double pause_s = 0.0)
{
    std::vector<QzLayerRecord> L;
    double t = 0.0;
    for (int i = 0; i < n; ++i) {
        QzLayerRecord r;
        r.z_bottom = i * h_mm * 1e-3; r.height = h_mm * 1e-3; r.thickness = w_mm * 1e-3;
        r.t_start = t; t += t_layer; r.t_end = t;
        if (pause_every && (i + 1) % pause_every == 0) t += pause_s;
        L.push_back(r);
    }
    return L;
}
}

QZ_TEST(stability_heavy_column_classic_7_837)
{
    // uniform EI = 1, q = 1, L = 1  ->  q_cr L^3 / EI = 7.837 (clamped-free column under self weight)
    const double lam = qz_heavy_column_load_factor([](double) { return 1.0; }, 1.0, 1.0);
    QZ_CHECK_NEAR(lam, 7.837, 0.02);
}

QZ_TEST(stability_plastic_collapse_at_sigma_over_rho_g_without_curing)
{
    QzPasteMaterial m; m.rho = 1000.0; m.tau0 = 1000.0 / std::sqrt(3.0); m.athix = 0.0; m.E0 = 1e9;
    QzStabilityOptions o; o.base_confinement = false;
    const auto r = qz_evaluate_stability(m, uniform_layers(400, 1.0, 1.0, 10.0), o);
    QZ_CHECK(r.valid);
    // sigma_p = 1000 Pa -> H = 1000 / (1000 * 9.81) = 0.1019 m -> collapse when 102-103 layers are on top of the base
    QZ_CHECK(r.collapse_after_layer >= 101 && r.collapse_after_layer <= 103);
    QZ_CHECK(r.critical_layer == 0);
    QZ_CHECK_NEAR(r.max_height_plastic, 0.10194, 1e-4);
}

QZ_TEST(stability_base_confinement_moves_critical_layer_up)
{
    QzPasteMaterial m; m.rho = 1000.0; m.tau0 = 1000.0 / std::sqrt(3.0); m.athix = 0.0;
    QzStabilityOptions on; on.base_confinement = true;
    QzStabilityOptions off; off.base_confinement = false;
    const auto L = uniform_layers(400, 1.0, 1.0, 10.0);
    const auto a = qz_evaluate_stability(m, L, on);
    const auto b = qz_evaluate_stability(m, L, off);
    QZ_CHECK(a.critical_layer > 0);
    QZ_CHECK(a.collapse_after_layer > b.collapse_after_layer);
}

QZ_TEST(stability_structuration_faster_than_loading_never_collapses)
{
    QzPasteMaterial m; m.rho = 1000.0; m.tau0 = 500.0; m.athix = 50.0;
    const auto r = qz_evaluate_stability(m, uniform_layers(400, 1.0, 1.0, 10.0), QzStabilityOptions{});
    QZ_CHECK(r.valid && r.collapse_after_layer < 0);
    QZ_CHECK(qz_max_height_plastic_linear(m, 1e-3) < 0.0);          // unbounded
    QZ_CHECK_NEAR(qz_critical_build_rate(m) * 1e3, 1.7320508 * 50.0 / 9.81, 1e-3); // mm/s
}

QZ_TEST(stability_pauses_extend_the_reachable_height)
{
    QzPasteMaterial m; m.rho = 1500.0; m.tau0 = 550.0; m.athix = 0.04;
    const auto a = qz_evaluate_stability(m, uniform_layers(80, 3.0, 125.0, 8.0), QzStabilityOptions{});
    const auto b = qz_evaluate_stability(m, uniform_layers(80, 3.0, 125.0, 8.0, 10, 600.0), QzStabilityOptions{});
    QZ_CHECK(a.collapse_after_layer > 0);
    QZ_CHECK(b.collapse_after_layer > a.collapse_after_layer);
    QZ_CHECK(a.critical_layer > 0); // band above the base (confinement on by default)
}

QZ_TEST(stability_min_time_scale_makes_schedule_safe)
{
    QzPasteMaterial m; m.rho = 1500.0; m.tau0 = 550.0; m.athix = 0.04;
    QzStabilityOptions o; o.safety_factor = 1.2;
    const auto L = uniform_layers(60, 3.0, 125.0, 8.0);
    const auto r = qz_evaluate_stability(m, L, o);
    QZ_CHECK(r.min_time_scale > 1.0);
    auto L2 = L;
    for (auto &l : L2) { l.t_start *= r.min_time_scale; l.t_end *= r.min_time_scale; }
    const auto r2 = qz_evaluate_stability(m, L2, o);
    QZ_CHECK(r2.max_utilization <= 1.0 / 1.2 + 1e-3);
}

QZ_TEST(stability_identification_recovers_material)
{
    QzPasteMaterial m; m.rho = 1500.0; m.tau0 = 800.0; m.athix = 0.05;
    const double r1 = 0.02e-3, r2 = 0.05e-3;
    const double H1 = qz_max_height_plastic_linear(m, r1), H2 = qz_max_height_plastic_linear(m, r2);
    double tau0 = 0, athix = 0;
    QZ_CHECK(qz_identify_two_cylinder_tests(1500.0, m.kp, H1, r1, H2, r2, tau0, athix));
    QZ_CHECK_NEAR(tau0, 800.0, 0.5);
    QZ_CHECK_NEAR(athix, 0.05, 1e-4);
}

QZ_TEST(stability_buckling_closed_form_and_geometry)
{
    QzPasteMaterial m; m.rho = 1500.0; m.tau0 = 550.0; m.E0 = 30000.0; m.nu = 0.3;
    const double h = 8e-3;
    const double H0 = qz_buckling_height_free_wall_closed_form(m, h);
    QZ_CHECK_NEAR(H0, std::cbrt(7.837 * m.E0 * h * h / (12.0 * (1 - 0.09) * m.rho * 9.81)), 1e-6);
    const double Hn = qz_buckling_height_free_wall(m, h, 0.024e-3);            // numeric, no curing (xiE = 0)
    QZ_CHECK_NEAR(Hn, H0, 0.01 * H0);
    const double Hc = qz_buckling_height_free_wall(m, h, 0.024e-3, 10e-3);     // corrugated: I x ~10 -> H x ~2.2
    QZ_CHECK(Hc > 2.0 * H0 && Hc < 2.5 * H0);
    m.xiE = 2e-4;
    QZ_CHECK(qz_buckling_height_free_wall(m, h, 0.024e-3) > Hn);               // age stiffening helps
    QZ_CHECK(qz_cylinder_shell_buckling_height(m, h, 30e-3) > 0.05);
}

QZ_TEST(stability_layers_from_moves_sorted_and_timed)
{
    std::vector<QzMoveSample> mv;
    // start gcode travels (layer 0, no extrusion), then 3 layers with a dwell inside layer 2
    mv.push_back({0.f, 0.f, 0.f, 5.f, 0, false});
    for (unsigned k = 1; k <= 3; ++k) {
        mv.push_back({3.f * k, 0.f, 0.f, 1.f, k, false});                 // travel to layer
        mv.push_back({3.f * k, 3.f, 4.f, 10.f, k, true});
        mv.push_back({3.f * k, 3.f, 4.f, 10.f, k, true});
        if (k == 2) mv.push_back({3.f * k, 0.f, 0.f, 60.f, k, false});   // pause / refill dwell
    }
    std::vector<int> idx;
    const auto L = qz_layers_from_moves(mv, idx);
    QZ_CHECK(L.size() == 3);
    QZ_CHECK(idx.size() == 4 && idx[0] == -1 && idx[1] == 0 && idx[3] == 2);
    QZ_CHECK_NEAR(L[0].z_bottom, 0.0, 1e-9);
    QZ_CHECK_NEAR(L[0].height, 3e-3, 1e-9);
    QZ_CHECK_NEAR(L[0].thickness, 4e-3, 1e-9);
    QZ_CHECK_NEAR(L[0].t_start, 6.0, 1e-6);       // 5 s start + 1 s travel
    QZ_CHECK_NEAR(L[1].t_end - L[1].t_start, 80.0, 1e-6);  // 2 x 10 s + 60 s dwell
    QZ_CHECK(L[2].t_start > L[1].t_end - 1e-9);
}

QZ_TEST(stability_uncharacterised_material_is_invalid_not_a_crash)
{
    QzPasteMaterial m; m.tau0 = 0.0;
    const auto r = qz_evaluate_stability(m, uniform_layers(10, 3.0, 100.0, 4.0), QzStabilityOptions{});
    QZ_CHECK(!r.valid);
}

// ---------------------------------------------------------------- Level 1.5 kinematics
namespace {
// ring (cylinder) or straight wall geometry per layer, in metres
std::vector<QzLayerGeom> ring_geom(int n, double R, double w)
{
    std::vector<QzLayerGeom> g(n);
    for (auto &x : g) { x.cx = 0.05; x.cy = 0.05; x.area = 2 * 3.14159265 * R * w; x.I_min = 3.14159265 * R * R * R * w; x.dir_x = 1; x.dir_y = 0; x.r_max = R; }
    return g;
}
std::vector<QzLayerGeom> wall_geom(int n, double Lw, double w)
{
    std::vector<QzLayerGeom> g(n);
    for (auto &x : g) { x.cx = 0.05; x.cy = 0.05; x.area = Lw * w; x.I_min = Lw * w * w * w / 12.0; x.dir_x = 0; x.dir_y = 1; x.r_max = 0.5 * Lw; }
    return g;
}
}

QZ_TEST(deform_cylinder_resists_buckling_wall_does_not)
{
    QzPasteMaterial m; m.rho = 1500.0; m.tau0 = 550.0; m.athix = 0.04; m.E0 = 30000.0;
    const auto L = uniform_layers(40, 3.0, 125.0, 8.0);
    const auto lam_ring = qz_buckling_load_factors(m, L, ring_geom(40, 0.03, 0.008));
    const auto lam_wall = qz_buckling_load_factors(m, L, wall_geom(40, 0.12, 0.004));
    // closed ring (Euler, E0 = 30 kPa): lambda ~ 4-5 at 120 mm, i.e. global buckling would
    // only come at ~200 mm, well above the plastic limit; the thin wall is long gone
    QZ_CHECK(lam_ring.back() > 3.0);
    QZ_CHECK(lam_wall.back() < 1.0);
    QZ_CHECK(lam_ring.back() > 5.0 * lam_wall.back());
    // the wall buckles somewhere before the top
    int first = -1; for (size_t k = 0; k < lam_wall.size(); ++k) if (lam_wall[k] <= 1.0) { first = (int) k; break; }
    QZ_CHECK(first > 3 && first < 40);
}

QZ_TEST(deform_state_bulges_at_critical_band_and_folds_after_collapse)
{
    QzPasteMaterial m; m.rho = 1500.0; m.tau0 = 550.0; m.athix = 0.04; m.E0 = 30000.0;
    const auto L = uniform_layers(60, 3.0, 125.0, 8.0);
    const auto G = ring_geom(60, 0.03, 0.008);
    const auto res = qz_evaluate_stability(m, L, QzStabilityOptions{});
    QZ_CHECK(res.collapse_after_layer > 0);
    const auto lam = qz_buckling_load_factors(m, L, G);
    QzDeformOptions o;
    // early: nothing squashed, no fold
    auto s0 = qz_deformation_state(m, L, G, res, lam, 5, L[5].t_end, o);
    QZ_CHECK(s0.valid && !s0.collapsed && s0.fold_angle == 0.0);
    for (const auto &d : s0.layers) QZ_CHECK(d.squash == 0.0 && d.scale_xy == 1.0);
    // just before collapse: squash concentrated at the critical band, above the base
    const int kb = res.collapse_after_layer - 1;
    auto s1 = qz_deformation_state(m, L, G, res, lam, kb, L[kb].t_end, o);
    int jmax = 0; for (size_t j = 0; j < s1.layers.size(); ++j) if (s1.layers[j].squash > s1.layers[jmax].squash) jmax = (int) j;
    QZ_CHECK(s1.layers[jmax].squash > 0.05 && jmax > 0 && jmax < kb / 2);
    QZ_CHECK(s1.height_deformed < L[kb].z_bottom + L[kb].height - L[0].z_bottom); // shorter than nominal
    // after collapse + fold time: hinge at the critical layer, top folded towards dir
    const int kc = res.collapse_after_layer;
    auto s2 = qz_deformation_state(m, L, G, res, lam, kc + 2, L[kc + 2].t_end + 1000.0, o);
    QZ_CHECK(s2.collapsed && !s2.by_buckling && s2.hinge_layer == res.critical_layer);
    QZ_CHECK(s2.fold_angle > 1.0); // ~75 deg
    double ox, oy, oz;
    const double ztop = L[kc + 2].z_bottom + L[kc + 2].height;
    qz_deform_point(s2, L, G, kc + 2, G[0].cx, G[0].cy, ztop, ox, oy, oz);
    QZ_CHECK(ox > G[0].cx + 0.02);      // moved towards +x (dir)
    QZ_CHECK(oz < ztop);                // and came down
    // a point below the hinge is only squashed/bulged, not rotated
    qz_deform_point(s2, L, G, 0, G[0].cx + 0.03, G[0].cy, L[0].z_bottom + L[0].height, ox, oy, oz);
    QZ_CHECK_NEAR(oy, G[0].cy, 1e-9);
    QZ_CHECK(oz <= L[0].z_bottom + L[0].height + 1e-9);
}
