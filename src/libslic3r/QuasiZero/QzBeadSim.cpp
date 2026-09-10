// Quasizero Slicer - QzBeadSim implementation. GNU AGPLv3.
#include "QzBeadSim.hpp"

#include <algorithm>
#include <cmath>

namespace Slic3r { namespace QuasiZero {

namespace {
inline uint32_t hash_u32(uint32_t v)
{
    v ^= v >> 16; v *= 0x7feb352dU; v ^= v >> 15; v *= 0x846ca68bU; v ^= v >> 16;
    return v;
}
} // namespace

bool QzBeadSim::build(const std::vector<QzSimSegment> &path, double t_start, const std::vector<float> &land,
                      double x0, double y0, double cell, int nx, int ny, const QzBeadOptions &opt)
{
    m_valid = false;
    m_opt = opt;
    if (path.empty() || nx <= 0 || ny <= 0 || land.size() != (size_t) nx * ny || cell <= 0.0) return false;
    m_path = path;
    m_x0 = x0; m_y0 = y0; m_cell = cell; m_nx = nx; m_ny = ny;
    m_land0 = land;
    m_t_start = t_start;
    m_t0.resize(m_path.size());
    double total = 0.0, wsum = 0.0, hsum = 0.0;
    for (size_t i = 0; i < m_path.size(); ++i) {
        m_t0[i] = (float) (i == 0 ? t_start : std::max((double) m_path[i - 1].t_end, t_start));
        total += std::hypot(m_path[i].x1 - m_path[i].x0, m_path[i].y1 - m_path[i].y0);
        wsum += m_path[i].w; hsum += m_path[i].h;
    }
    m_t_end = std::min((double) m_path.back().t_end, t_start + std::max(0.0, opt.max_duration));
    m_w = (float) (wsum / (double) m_path.size());
    m_h = (float) (hsum / (double) m_path.size());
    if (m_w <= 0.0f || m_h <= 0.0f) return false;
    m_spacing = std::max(opt.spacing_beads * (double) m_w, total / (double) std::max<size_t>(1, opt.max_particles));
    m_valid = true;
    reset();
    return true;
}

void QzBeadSim::reset()
{
    m_time = m_t_start;
    m_p.clear(); m_chains.clear(); m_active_chain = -1;
    m_next_seg = 0; m_seg_s = 0.0; m_seg_fresh = true;
    m_land = m_land0;
    m_live0.clear();
    m_snaps.clear();
}

float QzBeadSim::field_at(float x, float y) const
{
    int ix = (int) std::floor((x - m_x0) / m_cell), iy = (int) std::floor((y - m_y0) / m_cell);
    ix = std::max(0, std::min(m_nx - 1, ix)); iy = std::max(0, std::min(m_ny - 1, iy));
    return m_land[(size_t) iy * m_nx + ix];
}

void QzBeadSim::stamp(const QzBeadParticle &q, const QzBeadParticle *prev, const QzBeadParticle *next)
{
    // the bead at rest: flattened section, raises the field under its footprint - except
    // under its own chain neighbours, which lie alongside and must not be lifted onto it
    const float hf = m_h / (float) m_opt.flatten, wf = m_w * (float) m_opt.flatten;
    const int rc = std::max(0, (int) std::ceil(0.5 * wf / m_cell - 0.5));
    const int ix = (int) std::floor((q.x - m_x0) / m_cell), iy = (int) std::floor((q.y - m_y0) / m_cell);
    const float top = q.z + 0.5f * hf;
    const float keep2 = (0.6f * wf) * (0.6f * wf);
    for (int dy = -rc; dy <= rc; ++dy)
        for (int dx = -rc; dx <= rc; ++dx) {
            const int cx = ix + dx, cy = iy + dy;
            if (cx < 0 || cy < 0 || cx >= m_nx || cy >= m_ny) continue;
            const float ccx = (float) (m_x0 + (cx + 0.5) * m_cell), ccy = (float) (m_y0 + (cy + 0.5) * m_cell);
            bool near_neighbour = false;
            for (const QzBeadParticle *n : { prev, next })
                if (n != nullptr && !n->frozen && (n->x - ccx) * (n->x - ccx) + (n->y - ccy) * (n->y - ccy) < keep2) near_neighbour = true;
            if (near_neighbour) continue;
            float &f = m_land[(size_t) cy * m_nx + cx];
            f = std::max(f, top);
        }
}

float QzBeadSim::noise(float t, uint32_t salt) const
{
    // smooth, deterministic 1-D noise in [-1, 1]: three hashed sinusoids
    float v = 0.0f;
    for (uint32_t k = 0; k < 3; ++k) {
        const uint32_t hsh = hash_u32(m_opt.seed ^ salt ^ (k * 0x9E3779B9u));
        const float freq = 0.7f + 1.3f * (float) (hsh & 0xFFFF) / 65535.0f;
        const float phase = 6.2831853f * (float) ((hsh >> 16) & 0xFFFF) / 65535.0f;
        v += std::sin(freq * t + phase) / (float) (k + 1);
    }
    return v / 1.83f;
}

void QzBeadSim::nozzle_at(double t, float &x, float &y, float &z, float &vx, float &vy, bool &extruding) const
{
    extruding = false; vx = vy = 0.0f;
    if (m_path.empty()) { x = y = z = 0.0f; return; }
    if (t >= (double) m_path.back().t_end) { const QzSimSegment &s = m_path.back(); x = s.x1; y = s.y1; z = s.z; return; }
    // segment containing t (m_t0 is non-decreasing)
    size_t lo = 0, hi = m_path.size();
    while (hi - lo > 1) { const size_t mid = (lo + hi) / 2; if ((double) m_t0[mid] <= t) lo = mid; else hi = mid; }
    const QzSimSegment &s = m_path[lo];
    const double dur = (double) s.t_end - (double) m_t0[lo];
    const double u = dur > 1e-9 ? std::min(1.0, std::max(0.0, (t - (double) m_t0[lo]) / dur)) : 1.0;
    x = (float) (s.x0 + u * (s.x1 - s.x0)); y = (float) (s.y0 + u * (s.y1 - s.y0)); z = s.z;
    if (dur > 1e-9) { vx = (float) ((s.x1 - s.x0) / dur); vy = (float) ((s.y1 - s.y0) / dur); }
    extruding = true;
}

void QzBeadSim::emit_until(double t)
{
    while (m_next_seg < m_path.size()) {
        const QzSimSegment &s = m_path[m_next_seg];
        const double len = std::hypot(s.x1 - s.x0, s.y1 - s.y0);
        const double dur = std::max(1e-9, (double) s.t_end - (double) m_t0[m_next_seg]);
        // a travel before this segment breaks the chain (a plain layer change does not: the
        // nozzle only rises and the bead stays continuous)
        if (m_seg_fresh) {
            m_seg_fresh = false;
            if (m_next_seg > 0) {
                const QzSimSegment &p = m_path[m_next_seg - 1];
                if (std::hypot(s.x0 - p.x1, s.y0 - p.y1) > 1.5 * m_w || std::fabs(s.z - p.z) > 2.5 * m_h) { m_active_chain = -1; m_seg_s = 0.0; }
            }
        }
        // the emission cursor carries its remainder into the next segment (segments are
        // usually shorter than the particle spacing)
        if (m_seg_s >= len) { m_seg_s -= len; m_next_seg++; m_seg_fresh = true; continue; }
        const double t_emit = (double) m_t0[m_next_seg] + (len > 1e-9 ? m_seg_s / len : 0.0) * dur;
        if (t_emit > t || t_emit > m_t_end) return;
        if (m_p.size() >= m_opt.max_particles) { m_next_seg = m_path.size(); return; }
        const double u = len > 1e-9 ? m_seg_s / len : 0.0;
        QzBeadParticle q;
        q.x = (float) (s.x0 + u * (s.x1 - s.x0)); q.y = (float) (s.y0 + u * (s.y1 - s.y0)); q.z = s.z;
        q.t_emit = (float) t_emit;
        // initial velocity: the nozzle's, plus a little smooth lateral noise (the bead leaves
        // the nozzle wobbling, never perfectly along the path)
        const float vx = (float) ((s.x1 - s.x0) / dur), vy = (float) ((s.y1 - s.y0) / dur);
        const float vn = std::hypot(vx, vy);
        const float nx = vn > 1e-6f ? -vy / vn : 1.0f, ny = vn > 1e-6f ? vx / vn : 0.0f;
        const float lateral = (float) m_opt.noise_amp * vn * noise((float) t_emit, 0x51u);
        const float dt = (float) m_opt.dt;
        q.px = q.x - (vx + nx * lateral) * dt; q.py = q.y - (vy + ny * lateral) * dt; q.pz = q.z;
        if (m_active_chain < 0) { m_chains.emplace_back(); m_active_chain = (int) m_chains.size() - 1; }
        q.chain = m_active_chain;
        m_chains[(size_t) m_active_chain].push_back((int) m_p.size());
        m_p.push_back(q);
        m_seg_s += m_spacing;
    }
}

void QzBeadSim::step()
{
    const float dt = (float) m_opt.dt, g = (float) m_opt.gravity, damp = (float) m_opt.damping;
    const float half_h = 0.5f * m_h;
    m_time += dt;
    emit_until(m_time);
    // simulated duration exhausted while the head goes on: the strand is let go (it is not
    // dragged around without new material behind it)
    if (m_active_chain >= 0 && m_time > m_t_end && m_t_end < (double) m_path.back().t_end) m_active_chain = -1;
    // integrate
    for (QzBeadParticle &q : m_p) {
        if (q.frozen) continue;
        const float vx = (q.x - q.px) * damp, vy = (q.y - q.py) * damp, vz = (q.z - q.pz) * damp;
        q.px = q.x; q.py = q.y; q.pz = q.z;
        q.x += vx; q.y += vy; q.z += vz - g * dt * dt;
    }
    // contact state of every particle for this step: the strand already lying on the pile is
    // plastic - it resists being dragged by the hanging part (friction + yield), so it takes a
    // small share of the link corrections; the free strand takes the rest
    m_touch.assign(m_p.size(), 0);
    for (size_t i = 0; i < m_p.size(); ++i) {
        const QzBeadParticle &q = m_p[i];
        if (!q.frozen && q.z <= field_at(q.x, q.y) + half_h + 0.05f * m_h) m_touch[i] = 1;
    }
    auto weight = [&](size_t idx) -> float { const QzBeadParticle &q = m_p[idx]; return q.frozen ? 0.0f : (m_touch[idx] ? (float) m_opt.laid_share : 1.0f); };
    // the frozen head of every chain (it freezes in emission order, roughly) takes no part in
    // the constraints: only the links from its first live particle on are solved
    m_live0.resize(m_chains.size(), 0);
    for (size_t ci = 0; ci < m_chains.size(); ++ci) {
        const std::vector<int> &ch = m_chains[ci];
        size_t &k0 = m_live0[ci];
        k0 = std::min(k0, ch.size());
        while (k0 < ch.size() && m_p[(size_t) ch[k0]].frozen) ++k0;
    }
    // constraints
    float nx, ny, nz, nvx, nvy; bool extruding;
    nozzle_at(m_time, nx, ny, nz, nvx, nvy, extruding);
    const float sp = (float) m_spacing;
    for (int it = 0; it < m_opt.iterations; ++it) {
        for (size_t ci = 0; ci < m_chains.size(); ++ci) {
            const std::vector<int> &ch = m_chains[ci];
            const size_t k_first = std::max<size_t>(1, m_live0[ci]);   // link(k) touches k-1 and k
            if (ch.empty() || m_live0[ci] >= ch.size()) continue;
            // rope to the nozzle for the attached chain (pull only; it stays attached when the
            // path ends - the head stops and the bead hangs from it), then the links from the
            // nozzle end backwards so the anchored end drives the hanging strand
            if ((int) ci == m_active_chain) {
                QzBeadParticle &b = m_p[(size_t) ch.back()];
                if (!b.frozen) {
                    const float dx = nx - b.x, dy = ny - b.y, dz = nz - b.z;
                    const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
                    if (d > sp) { const float corr = (d - sp) / d; b.x += corr * dx; b.y += corr * dy; b.z += corr * dz; }
                }
            }
            auto link = [&](size_t k) {
                const size_t ia = (size_t) ch[k - 1], ib = (size_t) ch[k];
                QzBeadParticle &a = m_p[ia], &b = m_p[ib];
                const float wa0 = weight(ia), wb0 = weight(ib), ws = wa0 + wb0;
                if (ws <= 0.0f) return;
                const float dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
                const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (d < 1e-6f) return;
                const float corr = (d - sp) / d;
                const float wa = wa0 / ws, wb = wb0 / ws;
                if (wa > 0.0f) { a.x += wa * corr * dx; a.y += wa * corr * dy; a.z += wa * corr * dz; }
                if (wb > 0.0f) { b.x -= wb * corr * dx; b.y -= wb * corr * dy; b.z -= wb * corr * dz; }
            };
            for (size_t k = ch.size() - 1; k >= k_first; --k) link(k);   // from the nozzle end
            for (size_t k = k_first; k < ch.size(); ++k) link(k);        // and back
            // weak bending of the hanging strand (no kinks in the air); what lies on the pile
            // keeps the shape it fell into (plastic)
            if (m_opt.bend_stiff > 0.0)
                for (size_t k = k_first; k + 1 < ch.size(); ++k) {
                    const size_t ib = (size_t) ch[k];
                    QzBeadParticle &a = m_p[(size_t) ch[k - 1]], &b = m_p[ib], &c = m_p[(size_t) ch[k + 1]];
                    if (b.frozen || m_touch[ib]) continue;
                    const float mx = 0.5f * (a.x + c.x), my = 0.5f * (a.y + c.y), mz = 0.5f * (a.z + c.z);
                    const float s = (float) m_opt.bend_stiff * 0.25f;
                    b.x += s * (mx - b.x); b.y += s * (my - b.y); b.z += s * (mz - b.z);
                }
        }
        // ground / pile collision with friction. A step of the field much taller than a bead is
        // a wall (the side of the standing stack, a cliff of the fallen pile): the strand is
        // stopped against it instead of being lifted onto it
        const float wall_h = (float) m_opt.wall_beads * m_h;
        for (QzBeadParticle &q : m_p) {
            if (q.frozen) continue;
            float floor_z = field_at(q.x, q.y) + half_h;
            if (q.z < floor_z - wall_h) {
                q.x = q.px; q.y = q.py;
                floor_z = field_at(q.x, q.y) + half_h;
            }
            if (q.z < floor_z) {
                q.z = floor_z;
                // friction: kill the horizontal motion on contact
                q.px = q.x; q.py = q.y;
                q.pz = std::max(q.pz, floor_z);
            }
        }
    }
    // rest detection and freezing (stamped into the field so later strands pile on it)
    for (size_t ci = 0; ci < m_chains.size(); ++ci) {
        const std::vector<int> &ch = m_chains[ci];
        for (size_t k = m_live0[ci]; k < ch.size(); ++k) {
            QzBeadParticle &q = m_p[(size_t) ch[k]];
            if (q.frozen) continue;
            const float sp2 = (q.x - q.px) * (q.x - q.px) + (q.y - q.py) * (q.y - q.py) + (q.z - q.pz) * (q.z - q.pz);
            const bool touching = q.z <= field_at(q.x, q.y) + half_h + 0.05f * m_h;
            if (touching && sp2 < 1e-4f * m_h * m_h) {
                if (q.rest_since < 0.0f) q.rest_since = (float) m_time;
                else if (m_time - (double) q.rest_since >= m_opt.rest_time) {
                    q.frozen = true;
                    stamp(q, k > 0 ? &m_p[(size_t) ch[k - 1]] : nullptr, k + 1 < ch.size() ? &m_p[(size_t) ch[k + 1]] : nullptr);
                }
            } else
                q.rest_since = -1.0f;
        }
    }
}

void QzBeadSim::seek(double t)
{
    if (!m_valid) return;
    t = std::max(t, m_t_start);
    if (t < m_time) {
        // rewind to the latest snapshot at or before t
        size_t si = m_snaps.size();
        while (si > 0 && m_snaps[si - 1].time > t) --si;
        if (si == 0) reset();
        else {
            const Snapshot &s = m_snaps[si - 1];
            m_time = s.time; m_p = s.p; m_land = s.land; m_next_seg = s.next_seg; m_seg_s = s.seg_s; m_seg_fresh = s.seg_fresh; m_chains = s.chains; m_active_chain = s.active_chain;
            m_live0.clear();   // rescanned on the next step
            m_snaps.resize(si);
        }
    }
    // beyond the end of the path nothing more is emitted, but the bead keeps settling
    const double t_stop = std::min(t, m_t_end + 5.0);
    size_t steps = 0;
    const double next_snap = m_snaps.empty() ? m_t_start + m_opt.snapshot_dt : m_snaps.back().time + m_opt.snapshot_dt;
    double snap_at = next_snap;
    while (m_time + 0.5 * m_opt.dt < t_stop && steps < 400000) {
        step(); ++steps;
        if (m_time >= snap_at) {
            m_snaps.push_back(Snapshot{ m_time, m_p, m_land, m_next_seg, m_seg_s, m_seg_fresh, m_chains, m_active_chain });
            snap_at += m_opt.snapshot_dt;
        }
    }
    // beyond the end of the path (+5 s of settling) the bead is at rest: just move the clock
    if (m_time + 0.5 * m_opt.dt >= t_stop && t > m_time) { m_time = t; emit_until(std::min(t, m_t_end)); }
}

bool QzBeadSim::nozzle(float &x, float &y, float &z) const
{
    if (!m_valid || m_time < m_t_start) return false;
    float vx, vy; bool ex;
    nozzle_at(m_time, x, y, z, vx, vy, ex);
    return true;
}

}} // namespace Slic3r::QuasiZero
