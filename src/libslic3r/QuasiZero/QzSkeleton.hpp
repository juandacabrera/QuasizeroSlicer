// Quasizero Slicer - QzSkeleton: the printed toolpath as beads (polylines of SHARED centre-line
// nodes), and QzTubeMesher: a continuous tube re-skin of a displaced skeleton. GNU AGPLv3.
//
// Why: OrcaSlicer's viewer keeps one tube per move with its own vertices, so any deformation
// applied per move splits the print into loose blocks (handoff E5). Here consecutive
// extrusion segments that share an endpoint (no travel in between) become one bead with one
// node at the junction; a displacement field is applied to the NODES and the tube is rebuilt
// with rings shared between neighbouring segments, so a bead bends, squashes and falls but
// never comes apart. Pure C++17, no dependencies (testable standalone).
#pragma once

#include "QzStackSim.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Slic3r { namespace QuasiZero {

struct QzSkelNode
{
    float x = 0, y = 0, z = 0;   // [mm] nominal centre-line point (bead top, as the toolpath)
    float w = 0, h = 0;          // [mm] bead width / height at this node
    int   layer = -1;            // stability record index
    float t = 0;                 // [s] process time when the nozzle passed here
};

struct QzSkelBead
{
    int first_node = 0;          // nodes [first_node, first_node + node_count)
    int node_count = 0;
    int first_seg  = 0;          // segments [first_seg, first_seg + node_count - 1), in the sim's order
};

class QzSkeleton
{
public:
    // Segments must be in deposition order (as QzStackSim stores them). Two consecutive
    // segments join when the end of one is the start of the next (within `join_tol` mm).
    void build(const std::vector<QzSimSegment> &segs, float join_tol = 1e-3f);

    const std::vector<QzSkelNode> &nodes() const { return m_nodes; }
    const std::vector<QzSkelBead> &beads() const { return m_beads; }
    // node ids of segment i (start, end)
    int seg_node_a(size_t i) const { return m_seg_a[i]; }
    int seg_node_b(size_t i) const { return m_seg_b[i]; }
    size_t segments_count() const { return m_seg_a.size(); }
    bool   empty() const { return m_nodes.empty(); }

private:
    std::vector<QzSkelNode> m_nodes;
    std::vector<QzSkelBead> m_beads;
    std::vector<int> m_seg_a, m_seg_b;
};

// Displaced node, produced by the deformation engine for one playback instant
struct QzSkelNodePose
{
    float x = 0, y = 0, z = 0;   // [mm] displaced bead top
    float w_scale = 1.0f, h_scale = 1.0f;
    float value = 0.0f;          // colour field (load/strength ratio, 0..1)
    bool  visible = true;        // false = not yet deposited at this instant
};

// Triangle soup for one colour bin (positions + normals + indices)
struct QzTubeGeometry
{
    std::vector<float>    positions; // xyz per vertex
    std::vector<float>    normals;   // xyz per vertex
    std::vector<uint32_t> indices;   // triangles
    size_t vertices_count() const { return positions.size() / 3; }
    void clear() { positions.clear(); normals.clear(); indices.clear(); }
};

struct QzTubeOptions
{
    int   sides = 8;             // ring resolution (4 for very large jobs)
    int   bins  = 16;            // colour bins, value in [0,1) -> bin
    float cap_ends = 1.0f;       // 1 = close bead ends
};

class QzTubeMesher
{
public:
    // Re-skin every bead of `skel` whose nodes are visible, into `out` (one geometry per
    // colour bin; resized to opt.bins). A bead is split at nodes that are not visible.
    // Rings are shared: the ring at node i is the same for segment i-1 and segment i (each
    // segment copies it into its own bin's geometry), so neighbouring segments join exactly.
    static void mesh(const QzSkeleton &skel, const std::vector<QzSkelNodePose> &pose,
                     const QzTubeOptions &opt, std::vector<QzTubeGeometry> &out);

    // number of connected components of the meshed tubes (by exact ring coincidence) -
    // test helper; equals the number of visible bead runs when the mesh is continuous
    static size_t connected_components(const QzSkeleton &skel, const std::vector<QzSkelNodePose> &pose,
                                       const QzTubeOptions &opt);
};

// Poses of every node for one simulator frame. Segments in [seg_lo, seg_hi] (sim order) are
// deposited/visible; the shared node between two visible segments gets the same position
// from both (the simulator transforms endpoints by their coordinates), its colour value is
// the max of the two segments' remembered load/strength.
void qz_pose_from_sim(const QzStackSim &sim, const QzSimFrame &fr, const QzSkeleton &skel,
                      int seg_lo, int seg_hi, std::vector<QzSkelNodePose> &out);

}} // namespace Slic3r::QuasiZero
