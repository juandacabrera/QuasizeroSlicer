// Quasizero Slicer - the toolpath skeleton and the tube re-skin, as the LITE tree has them
// (no simulator: poses are made up here). GNU AGPLv3.
#include "standalone/qz_test.hpp"
#include "libslic3r/QuasiZero/QzSkeleton.hpp"

#include <cmath>

using namespace Slic3r::QuasiZero;

namespace {
// two rings of 8 segments on two layers, a travel between them
std::vector<QzSimSegment> two_rings()
{
    std::vector<QzSimSegment> segs;
    for (int k = 0; k < 2; ++k)
        for (int s = 0; s < 8; ++s) {
            const double a0 = 2 * 3.14159265 * s / 8, a1 = 2 * 3.14159265 * (s + 1) / 8;
            QzSimSegment g;
            g.x0 = float(10 * std::cos(a0)); g.y0 = float(10 * std::sin(a0)); g.x1 = float(10 * std::cos(a1)); g.y1 = float(10 * std::sin(a1));
            g.z = float(3 * (k + 1)); g.w = 4; g.h = 3; g.layer = k; g.t_end = float(k * 8 + s + 1);
            segs.push_back(g);
        }
    return segs;
}
} // namespace

QZ_TEST(skeleton_lite_shares_nodes_along_a_bead_and_splits_at_travels)
{
    QzSkeleton sk;
    sk.build(two_rings());
    QZ_CHECK(sk.segments_count() == 16);
    QZ_CHECK(sk.beads().size() == 2);              // one bead per ring (the ring ends are not merged)
    QZ_CHECK(sk.nodes().size() == 2 * 9);          // 8 segments -> 9 nodes per ring
    QZ_CHECK(sk.seg_node_b(0) == sk.seg_node_a(1)); // consecutive segments share their node
    QZ_CHECK(sk.seg_node_b(7) != sk.seg_node_a(8)); // the travel to the next layer breaks the bead
    QZ_CHECK(sk.nodes()[0].layer == 0 && sk.nodes()[9].layer == 1 && sk.nodes()[0].w == 4.0f);
}

QZ_TEST(skeleton_lite_tube_mesher_skins_visible_nodes_and_free_polylines)
{
    QzSkeleton sk;
    sk.build(two_rings());
    std::vector<QzSkelNodePose> pose(sk.nodes().size());
    for (size_t n = 0; n < pose.size(); ++n) {
        const QzSkelNode &nd = sk.nodes()[n];
        pose[n].x = nd.x; pose[n].y = nd.y; pose[n].z = nd.z; pose[n].value = 0.3f; pose[n].visible = n < 9;   // first ring only
    }
    QzTubeOptions opt; opt.sides = 8; opt.bins = 4;
    std::vector<QzTubeGeometry> geos;
    QzTubeMesher::mesh(sk, pose, opt, geos);
    QZ_CHECK(geos.size() == 4);
    size_t verts = 0, tris = 0;
    for (const QzTubeGeometry &g : geos) { verts += g.vertices_count(); tris += g.indices.size() / 3; }
    QZ_CHECK(verts > 0 && tris > 0);
    QZ_CHECK(geos[1].vertices_count() > 0 && geos[0].vertices_count() == 0);   // value 0.3 -> bin 1 of 4
    QZ_CHECK(QzTubeMesher::connected_components(sk, pose, opt) == 1);
    // a free polyline (the post-collapse bead) appends to the given bin
    std::vector<QzSimPoint> chain = { { 0, 0, 3 }, { 5, 0, 3 }, { 10, 2, 3.5f } };
    const size_t before = geos[0].vertices_count();
    QzTubeMesher::mesh_polyline(chain, 4.0f, 3.0f, 0.02f, opt, geos);
    QZ_CHECK(geos[0].vertices_count() > before);
    // every normal is unit length
    for (const QzTubeGeometry &g : geos)
        for (size_t v = 0; v < g.vertices_count(); ++v) {
            const float nx = g.normals[3 * v], ny = g.normals[3 * v + 1], nz = g.normals[3 * v + 2];
            QZ_CHECK(std::fabs(std::sqrt(nx * nx + ny * ny + nz * nz) - 1.0f) < 1e-3f);
        }
}
