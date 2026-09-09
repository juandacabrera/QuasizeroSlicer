// Quasizero Slicer - QzSkeleton / QzTubeMesher tests. GNU AGPLv3.
#include "standalone/qz_test.hpp"
#include "libslic3r/QuasiZero/QzSkeleton.hpp"
#include <cmath>

using namespace Slic3r::QuasiZero;

namespace {
// two closed rings per layer (a travel between them), n_layers layers of 3 mm
std::vector<QzSimSegment> ring_segments(int n_layers, int nseg = 32, double cx = 100.0, double cy = 100.0)
{
    std::vector<QzSimSegment> segs;
    double t = 0.0;
    for (int k = 0; k < n_layers; ++k)
        for (double R : { 28.0, 32.0 })
            for (int s = 0; s < nseg; ++s) {
                const double a0 = 2 * 3.14159265 * s / nseg, a1 = 2 * 3.14159265 * (s + 1) / nseg;
                QzSimSegment sg;
                sg.x0 = (float) (cx + R * std::cos(a0)); sg.y0 = (float) (cy + R * std::sin(a0));
                sg.x1 = (float) (cx + R * std::cos(a1)); sg.y1 = (float) (cy + R * std::sin(a1));
                sg.z = (float) ((k + 1) * 3.0); sg.w = 4.0f; sg.h = 3.0f; sg.layer = k; t += 1.0; sg.t_end = (float) t;
                segs.push_back(sg);
            }
    return segs;
}

std::vector<QzSkelNodePose> nominal_pose(const QzSkeleton &sk)
{
    std::vector<QzSkelNodePose> pose(sk.nodes().size());
    for (size_t i = 0; i < pose.size(); ++i) {
        pose[i].x = sk.nodes()[i].x; pose[i].y = sk.nodes()[i].y; pose[i].z = sk.nodes()[i].z;
        pose[i].value = 0.3f; pose[i].visible = true;
    }
    return pose;
}
} // namespace

QZ_TEST(skeleton_shares_nodes_between_consecutive_segments)
{
    const auto segs = ring_segments(3);
    QzSkeleton sk; sk.build(segs);
    QZ_CHECK(sk.segments_count() == segs.size());
    QZ_CHECK(sk.beads().size() == 6);                       // 2 rings x 3 layers
    QZ_CHECK(sk.nodes().size() == 6 * 33);                  // 32 segments -> 33 nodes per ring
    for (size_t i = 1; i < segs.size(); ++i)
        if (segs[i].layer == segs[i - 1].layer && std::fabs(segs[i].x0 - segs[i - 1].x1) < 1e-4f && std::fabs(segs[i].y0 - segs[i - 1].y1) < 1e-4f)
            QZ_CHECK(sk.seg_node_a(i) == sk.seg_node_b(i - 1));   // shared node
    // the ring is not closed onto its first node (the start node stays distinct): a ring is a polyline
    QZ_CHECK(sk.seg_node_a(0) != sk.seg_node_b(31));
}

QZ_TEST(tube_mesh_is_one_component_per_bead_even_when_displaced)
{
    const auto segs = ring_segments(4);
    QzSkeleton sk; sk.build(segs);
    std::vector<QzSkelNodePose> pose = nominal_pose(sk);
    // an arbitrary smooth displacement + squash: bend everything sideways with height
    for (size_t i = 0; i < pose.size(); ++i) {
        pose[i].x += 0.4f * pose[i].z; pose[i].z *= 0.8f;
        pose[i].w_scale = 1.3f; pose[i].h_scale = 0.7f;
        pose[i].value = (float) (0.1 + 0.05 * (i % 16));         // spread over the colour bins
    }
    QzTubeOptions opt;
    QZ_CHECK(QzTubeMesher::connected_components(sk, pose, opt) == sk.beads().size());
    std::vector<QzTubeGeometry> geos;
    QzTubeMesher::mesh(sk, pose, opt, geos);
    size_t nv = 0, ni = 0;
    for (const QzTubeGeometry &g : geos) { nv += g.vertices_count(); ni += g.indices.size(); QZ_CHECK(g.normals.size() == g.positions.size()); }
    // per segment: 2 rings x sides vertices, 2*sides triangles; per bead: 2 caps of (sides+1) vertices, sides triangles
    const size_t nseg = segs.size(), nbead = sk.beads().size(), sides = (size_t) opt.sides;
    QZ_CHECK(nv == nseg * 2 * sides + nbead * 2 * (sides + 1));
    QZ_CHECK(ni == 3 * (nseg * 2 * sides + nbead * 2 * sides));
    // every index is in range
    for (const QzTubeGeometry &g : geos) for (uint32_t idx : g.indices) QZ_CHECK(idx < g.vertices_count());
}

QZ_TEST(tube_mesh_splits_at_invisible_nodes_and_skips_hidden_beads)
{
    const auto segs = ring_segments(2);
    QzSkeleton sk; sk.build(segs);
    std::vector<QzSkelNodePose> pose = nominal_pose(sk);
    // hide the whole second layer and the middle of the first bead
    for (size_t i = 0; i < pose.size(); ++i) if (sk.nodes()[i].layer == 1) pose[i].visible = false;
    const QzSkelBead &b0 = sk.beads()[0];
    // hide node 10 (splits the ring in two runs) and node 0 (otherwise the end caps of the
    // two runs meet at the ring closure, where the first and last node coincide)
    pose[(size_t) b0.first_node + 10].visible = false;
    pose[(size_t) b0.first_node + 0].visible = false;
    QzTubeOptions opt;
    // layer 0: bead 0 split in two runs + bead 1 whole = 3 components
    QZ_CHECK(QzTubeMesher::connected_components(sk, pose, opt) == 3);
    // and a closed ring drawn whole is one piece
    for (QzSkelNodePose &q : pose) q.visible = (q.z < 4.0f);
    QZ_CHECK(QzTubeMesher::connected_components(sk, pose, opt) == 2);
}

QZ_TEST(pose_from_sim_gives_shared_nodes_one_position_and_visibility_by_range)
{
    // a real simulator on a short ring job
    QzPasteMaterial mat; mat.rho = 1500.0; mat.tau0 = 550.0; mat.athix = 0.04; mat.E0 = 30000.0;
    std::vector<QzLayerRecord> L; std::vector<QzLayerGeom> G;
    const int n_layers = 20; double t = 0.0;
    for (int k = 0; k < n_layers; ++k) {
        QzLayerRecord r; r.z_bottom = k * 3e-3; r.height = 3e-3; r.thickness = 4e-3; r.t_start = t; t += 64.0; r.t_end = t; L.push_back(r);
        QzLayerGeom g; g.cx = 0.1; g.cy = 0.1; g.area = 2 * 3.14159265 * 0.030 * 0.008; g.I_min = 3.14159265 * 0.030 * 0.030 * 0.030 * 0.008;
        g.dir_x = 1; g.dir_y = 0; g.r_max = 0.034; G.push_back(g);
    }
    std::vector<QzSimSegment> segs = ring_segments(n_layers);
    for (size_t i = 0; i < segs.size(); ++i) segs[i].t_end = (float) (segs[i].layer * 64.0 + 64.0 * ((i % 64) + 1) / 64.0);
    const QzStabilityResult res = qz_evaluate_stability(mat, L, QzStabilityOptions{});
    const std::vector<double> lam = qz_buckling_load_factors(mat, L, G);
    QzStackSim sim; QZ_CHECK(sim.build(mat, L, G, segs, res, lam, QzStabilityOptions{}, QzSimOptions{}));
    QzSkeleton sk; sk.build(segs);
    QzSimFrame fr; sim.frame(15, L[15].t_end, fr);
    std::vector<QzSkelNodePose> pose;
    // visible: layers 0..15, and only the first 20 segments of layer 15
    const int seg_hi = 15 * 64 + 19;
    qz_pose_from_sim(sim, fr, sk, 0, seg_hi, pose);
    QZ_CHECK(pose.size() == sk.nodes().size());
    size_t vis = 0;
    for (size_t i = 0; i < pose.size(); ++i) {
        if (pose[i].visible) ++vis;
        const int layer = sk.nodes()[i].layer;
        if (layer > 15) QZ_CHECK(!pose[i].visible);
    }
    QZ_CHECK(vis > 0);
    // node shared by segments 5 and 6 (layer 0): one pose, taken once
    const int shared = sk.seg_node_b(5);
    QZ_CHECK(shared == sk.seg_node_a(6));
    QZ_CHECK(pose[(size_t) shared].visible);
    QZ_CHECK(pose[(size_t) shared].value >= sim.ratio(fr, 5) - 1e-6f && pose[(size_t) shared].value >= sim.ratio(fr, 6) - 1e-6f);
    // the mesh of the visible part is continuous: one component per visible bead run
    QzTubeOptions opt;
    // layers 0..14 -> 30 beads, layer 15 -> segments 0..19 of the first ring -> 1 run: 31 components
    QZ_CHECK(QzTubeMesher::connected_components(sk, pose, opt) == 31);
}
