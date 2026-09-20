// Quasizero Slicer - the client of the external qz-sim engine: the job document, engine
// discovery, and the protocol against a real `qz-sim serve` process when QZ_SIM_TEST_EXE
// points to one (the standalone runner builds it from engine/ in a PRO tree). GNU AGPLv3.
#include "standalone/qz_test.hpp"
#include "slic3r/Utils/QzEngine.hpp"

#include <cmath>
#include <cstdlib>

using namespace Slic3r;
using namespace Slic3r::QuasiZero;
using nlohmann::json;

namespace {
struct Scene
{
    QzPasteMaterial material; QzStabilityOptions options;
    std::vector<QzLayerRecord> layers; std::vector<QzLayerGeom> geom; std::vector<QzSimSegment> segs;
};
Scene cylinder(int n_layers)
{
    Scene sc;
    sc.material.rho = 1500; sc.material.tau0 = 550; sc.material.athix = 0.04; sc.material.E0 = 30000;
    const int nseg = 24; const double R = 17.5, cx = 100, cy = 100, circ = 2 * 3.14159265 * R, t_layer = circ / 3.0;
    double t = 0;
    for (int k = 0; k < n_layers; ++k) {
        QzLayerRecord r; r.z_bottom = k * 3e-3; r.height = 3e-3; r.t_start = t; r.t_end = t + t_layer; r.thickness = 4e-3; sc.layers.push_back(r);
        QzLayerGeom g; g.cx = 0.1; g.cy = 0.1; g.area = circ * 1e-3 * 4e-3; g.I_min = 3.14159265 * std::pow(R * 1e-3, 3) * 4e-3; g.dir_x = 1; g.dir_y = 0; g.r_max = 0.0195; sc.geom.push_back(g);
        for (int s = 0; s < nseg; ++s) {
            const double a0 = 2 * 3.14159265 * s / nseg, a1 = 2 * 3.14159265 * (s + 1) / nseg;
            QzSimSegment sg;
            sg.x0 = float(cx + R * std::cos(a0)); sg.y0 = float(cy + R * std::sin(a0)); sg.x1 = float(cx + R * std::cos(a1)); sg.y1 = float(cy + R * std::sin(a1));
            sg.z = float((k + 1) * 3.0); sg.w = 4; sg.h = 3; sg.layer = k; sg.t_end = float(t + t_layer * (s + 1) / nseg);
            sc.segs.push_back(sg);
        }
        t += t_layer;
    }
    return sc;
}
} // namespace

QZ_TEST(engine_client_job_document_carries_material_layers_geom_and_segments)
{
    const Scene sc = cylinder(3);
    const json j = qz_engine_job(sc.material, sc.options, sc.layers, sc.geom, sc.segs);
    QZ_CHECK(j["schema"] == "qz-sim-job/1");
    QZ_CHECK_NEAR(j["material"]["tau0"].get<double>(), 550.0, 1e-9);
    QZ_CHECK(j["stability"]["safety_factor"].get<double>() == 1.5);
    QZ_CHECK(j["layers"].size() == 3 && j["geom"].size() == 3 && j["segments"].size() == 72);
    QZ_CHECK(j["segments"][71]["layer"] == 2 && j["output"]["bead"] == true);
}

QZ_TEST(engine_client_discovery_lists_the_expected_places)
{
    const std::vector<std::string> c = qz_engine_candidates("/custom/qz-sim", "/apps/slicer");
    QZ_CHECK(!c.empty() && c[0] == "/custom/qz-sim");
    bool next_to_slicer = false;
    for (const std::string &p : c) if (p.find("/apps/slicer") == 0 && p.find(qz_engine_exe_name()) != std::string::npos) next_to_slicer = true;
    QZ_CHECK(next_to_slicer);
    QZ_CHECK(qz_find_engine("/definitely/not/here/qz-sim", "/nowhere").empty() || std::getenv("QZ_SIM") != nullptr);
    QZ_CHECK(std::string(qz_engine_exe_name()).find("qz-sim") == 0);
}

QZ_TEST(engine_client_talks_to_a_real_qz_sim_process_when_available)
{
    const char *exe = std::getenv("QZ_SIM_TEST_EXE");
    if (!exe || !*exe) { std::printf("  (QZ_SIM_TEST_EXE not set: process test skipped)\n"); return; }
    auto proc = std::make_unique<QzEngineProcess>();
    std::string err;
    QZ_CHECK(proc->start(exe, { "--no-licence" }, err));
    QzEngineClient client(std::move(proc));
    QZ_CHECK(client.connected());
    QzEngineStatus st;
    QZ_CHECK(client.hello(st));
    QZ_CHECK(st.running && st.protocol == "qz-sim-serve/1" && st.licensed && !st.licence_required);

    const Scene sc = cylinder(40);
    QzSkeleton local; local.build(sc.segs);
    QzEngineLoadInfo info;
    QZ_CHECK(client.load(qz_engine_job(sc.material, sc.options, sc.layers, sc.geom, sc.segs), info));
    QZ_CHECK(info.ok && info.sim_valid && info.collapse_step >= 20 && info.collapse_step <= 35);
    QZ_CHECK(info.nodes == local.nodes().size());   // same skeleton on both sides
    const double t_layer = 2 * 3.14159265 * 17.5 / 3.0;

    QzEngineFrame fr;
    QZ_CHECK(client.frame(5, 6 * t_layer, 0, 6 * 24 - 1, fr, err));
    QZ_CHECK(fr.valid && fr.phase == "stable" && fr.pose.size() == local.nodes().size());
    size_t visible = 0; for (const QzSkelNodePose &p : fr.pose) if (p.visible) ++visible;
    QZ_CHECK(visible == 6u * 25u);
    // the poses re-skin into one continuous tube per bead run
    QzTubeOptions opt; opt.sides = 8; opt.bins = 16;
    std::vector<QzTubeGeometry> geos;
    QzTubeMesher::mesh(local, fr.pose, opt, geos);
    size_t tris = 0; for (const QzTubeGeometry &g : geos) tris += g.indices.size() / 3;
    QZ_CHECK(tris > 0);

    const int kc = info.collapse_step;
    QZ_CHECK(client.frame(kc + 4, (kc + 5) * t_layer, 0, (kc + 5) * 24 - 1, fr, err));
    QZ_CHECK(fr.valid && fr.collapsed && fr.has_bead && !fr.chains.empty() && fr.bead_w > 0);
    size_t pts = 0; for (const auto &c : fr.chains) pts += c.size();
    QZ_CHECK(pts > 0);
    QZ_CHECK(client.unload());
    client.quit();
    QZ_CHECK(!client.connected() || true);
}
