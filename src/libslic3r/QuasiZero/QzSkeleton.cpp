// Quasizero Slicer - QzSkeleton / QzTubeMesher implementation. GNU AGPLv3.
#include "QzSkeleton.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>

namespace Slic3r { namespace QuasiZero {

static constexpr double QZ_PI2 = 6.28318530717958647692;

void QzSkeleton::build(const std::vector<QzSimSegment> &segs, float join_tol)
{
    m_nodes.clear(); m_beads.clear();
    m_seg_a.assign(segs.size(), -1); m_seg_b.assign(segs.size(), -1);
    const float tol2 = join_tol * join_tol;
    for (size_t i = 0; i < segs.size(); ++i) {
        const QzSimSegment &s = segs[i];
        bool joined = false;
        if (i > 0 && !m_beads.empty()) {
            const QzSimSegment &p = segs[i - 1];
            const float dx = s.x0 - p.x1, dy = s.y0 - p.y1, dz = s.z - p.z;
            joined = (m_seg_b[i - 1] >= 0) && (dx * dx + dy * dy + dz * dz <= tol2) && s.layer == p.layer;
        }
        if (!joined) {
            QzSkelBead b; b.first_node = (int) m_nodes.size(); b.first_seg = (int) i; b.node_count = 1;
            m_beads.push_back(b);
            QzSkelNode a; a.x = s.x0; a.y = s.y0; a.z = s.z; a.w = s.w; a.h = s.h; a.layer = s.layer;
            a.t = (i > 0) ? segs[i - 1].t_end : 0.0f; // the nozzle was here when the previous segment ended
            m_nodes.push_back(a);
        }
        m_seg_a[i] = (int) m_nodes.size() - 1;
        QzSkelNode n; n.x = s.x1; n.y = s.y1; n.z = s.z; n.w = s.w; n.h = s.h; n.layer = s.layer; n.t = s.t_end;
        m_nodes.push_back(n);
        m_seg_b[i] = (int) m_nodes.size() - 1;
        m_beads.back().node_count++;
    }
}

namespace {
struct V3 { float x, y, z; };
inline V3 operator+(V3 a, V3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline V3 operator-(V3 a, V3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline V3 operator*(V3 a, float k) { return { a.x * k, a.y * k, a.z * k }; }
inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(V3 a, V3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
inline float len(V3 a) { return std::sqrt(dot(a, a)); }
inline V3 norm(V3 a) { const float l = len(a); return l > 1e-9f ? a * (1.0f / l) : V3{ 0, 0, 1 }; }

struct Ring { std::vector<V3> p, n; };

void ring_at(const V3 &top, const V3 &T, V3 &side_io, float w, float h, int sides, Ring &r)
{
    // frame: side = T x Z (horizontal for planar paths); when T is (nearly) vertical keep the
    // previous side so the frame does not flip along the bead
    V3 side = cross(T, V3{ 0, 0, 1 });
    if (len(side) < 1e-3f) side = side_io;
    if (len(side) < 1e-3f) side = V3{ 1, 0, 0 };
    side = norm(side);
    if (dot(side, side_io) < 0.0f) side = side * -1.0f; // continuity of orientation
    side_io = side;
    const V3 up = norm(cross(side, T));
    const V3 c  = top - up * (0.5f * h);
    r.p.resize(sides); r.n.resize(sides);
    for (int s = 0; s < sides; ++s) {
        const float a = (float) (QZ_PI2 * (s + 0.5) / sides);
        const float ca = std::cos(a), sa = std::sin(a);
        r.p[s] = c + side * (0.5f * w * ca) + up * (0.5f * h * sa);
        r.n[s] = norm(side * (ca / std::max(1e-6f, 0.5f * w)) + up * (sa / std::max(1e-6f, 0.5f * h)));
    }
}

inline void add_v(QzTubeGeometry &g, const V3 &p, const V3 &n)
{
    g.positions.push_back(p.x); g.positions.push_back(p.y); g.positions.push_back(p.z);
    g.normals.push_back(n.x); g.normals.push_back(n.y); g.normals.push_back(n.z);
}
inline void add_t(QzTubeGeometry &g, uint32_t a, uint32_t b, uint32_t c)
{
    g.indices.push_back(a); g.indices.push_back(b); g.indices.push_back(c);
}

void add_cap(QzTubeGeometry &g, const Ring &r, const V3 &centre, const V3 &n, bool flip)
{
    const uint32_t base = (uint32_t) g.vertices_count();
    add_v(g, centre, n);
    for (const V3 &p : r.p) add_v(g, p, n);
    const int sides = (int) r.p.size();
    for (int s = 0; s < sides; ++s) {
        const uint32_t a = base + 1 + s, b = base + 1 + (s + 1) % sides;
        if (flip) add_t(g, base, b, a); else add_t(g, base, a, b);
    }
}
} // namespace

void QzTubeMesher::mesh(const QzSkeleton &skel, const std::vector<QzSkelNodePose> &pose,
                        const QzTubeOptions &opt, std::vector<QzTubeGeometry> &out)
{
    const int sides = std::max(3, opt.sides), bins = std::max(1, opt.bins);
    out.resize((size_t) bins);
    for (QzTubeGeometry &g : out) g.clear();
    if (pose.size() != skel.nodes().size()) return;
    const std::vector<QzSkelNode> &N = skel.nodes();
    std::vector<V3> P, T;
    std::vector<Ring> rings;
    for (const QzSkelBead &bd : skel.beads()) {
        int i = bd.first_node;
        const int end = bd.first_node + bd.node_count;
        while (i < end) {
            // next run of visible nodes
            while (i < end && !pose[i].visible) ++i;
            int j = i;
            while (j < end && pose[j].visible) ++j;
            const int m = j - i;
            if (m >= 2) {
                P.resize(m); T.resize(m); rings.resize(m);
                for (int k = 0; k < m; ++k) P[k] = V3{ pose[i + k].x, pose[i + k].y, pose[i + k].z };
                for (int k = 0; k < m; ++k) {
                    V3 t;
                    if (k == 0) t = P[1] - P[0];
                    else if (k == m - 1) t = P[m - 1] - P[m - 2];
                    else t = norm(P[k] - P[k - 1]) + norm(P[k + 1] - P[k]);
                    if (len(t) < 1e-6f) t = (k > 0) ? T[k - 1] : V3{ 1, 0, 0 };
                    T[k] = norm(t);
                }
                V3 side{ 0, 0, 0 };
                for (int k = 0; k < m; ++k) {
                    const QzSkelNode &nd = N[i + k];
                    ring_at(P[k], T[k], side, std::max(1e-3f, nd.w * pose[i + k].w_scale),
                            std::max(1e-3f, nd.h * pose[i + k].h_scale), sides, rings[k]);
                }
                // one quad strip per segment, in the segment's colour bin (mean of its nodes)
                for (int k = 0; k + 1 < m; ++k) {
                    const float v = 0.5f * (pose[i + k].value + pose[i + k + 1].value);
                    const int bin = std::min(bins - 1, (int) std::max(0.0f, std::min(v, 0.99999f) * (float) bins));
                    QzTubeGeometry &g = out[(size_t) bin];
                    const uint32_t base = (uint32_t) g.vertices_count();
                    for (int s = 0; s < sides; ++s) { add_v(g, rings[k].p[s], rings[k].n[s]); add_v(g, rings[k + 1].p[s], rings[k + 1].n[s]); }
                    for (int s = 0; s < sides; ++s) {
                        const uint32_t i0 = base + 2 * s, i1 = base + 2 * ((s + 1) % sides);
                        add_t(g, i0, i0 + 1, i1 + 1); add_t(g, i0, i1 + 1, i1);
                    }
                }
                if (opt.cap_ends > 0.0f) {
                    const float v0 = pose[i].value, v1 = pose[j - 1].value;
                    const int b0 = std::min(bins - 1, (int) std::max(0.0f, std::min(v0, 0.99999f) * (float) bins));
                    const int b1 = std::min(bins - 1, (int) std::max(0.0f, std::min(v1, 0.99999f) * (float) bins));
                    V3 c0{ 0, 0, 0 }, c1{ 0, 0, 0 };
                    for (const V3 &p : rings[0].p) c0 = c0 + p;
                    for (const V3 &p : rings[m - 1].p) c1 = c1 + p;
                    c0 = c0 * (1.0f / sides); c1 = c1 * (1.0f / sides);
                    add_cap(out[(size_t) b0], rings[0], c0, T[0] * -1.0f, true);
                    add_cap(out[(size_t) b1], rings[m - 1], c1, T[m - 1], false);
                }
            }
            i = j;
        }
    }
}

size_t QzTubeMesher::connected_components(const QzSkeleton &skel, const std::vector<QzSkelNodePose> &pose,
                                          const QzTubeOptions &opt)
{
    std::vector<QzTubeGeometry> geos;
    mesh(skel, pose, opt, geos);
    // union-find over vertices keyed by quantised position (shared rings coincide exactly)
    std::map<std::tuple<long, long, long>, int> key_to_id;
    std::vector<int> parent;
    auto find = [&](int a) { while (parent[a] != a) { parent[a] = parent[parent[a]]; a = parent[a]; } return a; };
    auto unite = [&](int a, int b) { a = find(a); b = find(b); if (a != b) parent[a] = b; };
    std::vector<std::vector<int>> ids(geos.size());
    for (size_t gi = 0; gi < geos.size(); ++gi) {
        const QzTubeGeometry &g = geos[gi];
        ids[gi].resize(g.vertices_count());
        for (size_t v = 0; v < g.vertices_count(); ++v) {
            const auto key = std::make_tuple((long) std::lround(g.positions[3 * v] * 1000.0), (long) std::lround(g.positions[3 * v + 1] * 1000.0),
                                             (long) std::lround(g.positions[3 * v + 2] * 1000.0));
            auto it = key_to_id.find(key);
            if (it == key_to_id.end()) { it = key_to_id.emplace(key, (int) parent.size()).first; parent.push_back((int) parent.size()); }
            ids[gi][v] = it->second;
        }
        for (size_t t = 0; t + 2 < g.indices.size(); t += 3) {
            unite(ids[gi][g.indices[t]], ids[gi][g.indices[t + 1]]);
            unite(ids[gi][g.indices[t]], ids[gi][g.indices[t + 2]]);
        }
    }
    size_t comps = 0;
    for (size_t a = 0; a < parent.size(); ++a) if (find((int) a) == (int) a) ++comps;
    return comps;
}

void qz_pose_from_sim(const QzStackSim &sim, const QzSimFrame &fr, const QzSkeleton &skel,
                      int seg_lo, int seg_hi, std::vector<QzSkelNodePose> &out)
{
    out.assign(skel.nodes().size(), QzSkelNodePose());
    for (QzSkelNodePose &p : out) p.visible = false;
    if (!sim.valid() || !fr.valid || skel.segments_count() != sim.segments_count()) return;
    seg_lo = std::max(0, seg_lo);
    seg_hi = std::min((int) skel.segments_count() - 1, seg_hi);
    auto put = [&](int node, const QzSimPoint &q, float ws, float hs, float v) {
        QzSkelNodePose &p = out[(size_t) node];
        if (p.visible) { p.value = std::max(p.value, v); return; } // shared node: same pose from both sides
        p.x = q.x; p.y = q.y; p.z = q.z; p.w_scale = ws; p.h_scale = hs; p.value = v; p.visible = true;
    };
    for (int s = seg_lo; s <= seg_hi; ++s) {
        const QzSimSegment &sg = sim.segment((size_t) s);
        if (sg.layer < 0 || sg.layer > fr.top) continue;
        QzSimPoint a, b; float ws, hs; bool fallen;
        sim.deform(fr, s, a, b, ws, hs, fallen);
        const float v = sim.ratio(fr, s);
        put(skel.seg_node_a((size_t) s), a, ws, hs, v);
        put(skel.seg_node_b((size_t) s), b, ws, hs, v);
    }
}

}} // namespace Slic3r::QuasiZero
