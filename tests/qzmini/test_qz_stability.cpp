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
