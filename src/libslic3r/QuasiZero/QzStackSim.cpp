// Quasizero Slicer - QzStackSim implementation. GNU AGPLv3.
#include "QzStackSim.hpp"

#include <algorithm>
#include <cmath>

namespace Slic3r { namespace QuasiZero {

static constexpr double QZ_PI = 3.14159265358979323846;

uint32_t QzStackSim::hash_u32(uint32_t v)
{
    v ^= v >> 16; v *= 0x7feb352dU; v ^= v >> 15; v *= 0x846ca68bU; v ^= v >> 16;
    return v;
}

int QzStackSim::cell_of(float x, float y) const
{
    int ix = (int) std::floor((x - m_x0) / m_cell);
    int iy = (int) std::floor((y - m_y0) / m_cell);
    ix = std::max(0, std::min(m_nx - 1, ix));
    iy = std::max(0, std::min(m_ny - 1, iy));
    return iy * m_nx + ix;
}

void QzStackSim::cells_along(float x0, float y0, float x1, float y1, float w, std::vector<int> &out) const
{
    const float len = std::hypot(x1 - x0, y1 - y0);
    const int   ns  = std::max(1, (int) std::ceil(len / (0.5f * (float) m_cell)));
    const int   rc  = std::max(0, (int) std::ceil(0.5 * w / m_cell - 0.5));
    for (int s = 0; s <= ns; ++s) {
        const float t = (float) s / (float) ns;
        const float x = x0 + t * (x1 - x0), y = y0 + t * (y1 - y0);
        const int ix = (int) std::floor((x - m_x0) / m_cell), iy = (int) std::floor((y - m_y0) / m_cell);
        for (int dy = -rc; dy <= rc; ++dy)
            for (int dx = -rc; dx <= rc; ++dx) {
                const int cx = ix + dx, cy = iy + dy;
                if (cx < 0 || cy < 0 || cx >= m_nx || cy >= m_ny) continue;
                out.push_back(cy * m_nx + cx);
            }
    }
}

float QzStackSim::squash_of(float U) const
{
    if (U <= (float) m_opt.U_yield) return 0.0f;
    const float f = std::min(1.0f, (U - (float) m_opt.U_yield) / std::max(1e-6f, 1.0f - (float) m_opt.U_yield));
    return (float) m_opt.eps_max * f * f;
}

float QzStackSim::U_inst(int layer, int cell, int k) const
{
    if (k < layer) return 0.0f;
    const size_t nc = (size_t) m_nx * m_ny;
    const float H = m_H[(size_t) k * nc + cell];
    const double z_top = (m_L[layer].z_bottom + m_L[layer].height) * 1e3;
    const double col = std::max(0.0, (double) H - z_top) * 1e-3;                   // [m] material above
    if (col <= 0.0) return 0.0f;
    const double stress   = m_mat.rho * QZ_G * col;
    double       strength = m_mat.sigma_p(m_L[k].t_end - m_L[layer].t_start);
    if (m_conf && m_conf_len > 0.0)
        strength *= 1.0 + (1.0 / (1.0 - m_mat.nu) - 1.0) * std::exp(-m_L[layer].z_bottom / m_conf_len);
    return strength > 0.0 ? (float) (stress / strength) : 1e6f;
}

float QzStackSim::U_peak(int layer, int cell, int k) const
{
    const size_t nc = (size_t) m_nx * m_ny;
    const int kp = m_kpeak[(size_t) layer * nc + cell];
    if (kp < 0) return 0.0f;
    if (k >= kp) return m_Upeak[(size_t) layer * nc + cell];
    return U_inst(layer, cell, k);
}

bool QzStackSim::build(const QzPasteMaterial &mat, const std::vector<QzLayerRecord> &layers,
                       const std::vector<QzLayerGeom> &geom, std::vector<QzSimSegment> segments,
                       const QzStabilityResult &res, const std::vector<double> &lambda_by_top,
                       const QzStabilityOptions &sopt, const QzSimOptions &opt)
{
    m_valid = false;
    if (!res.valid || layers.empty() || geom.size() != layers.size() || segments.empty()) return false;
    m_mat = mat; m_L = layers; m_G = geom; m_segs = std::move(segments); m_opt = opt;
    m_conf = sopt.base_confinement;
    m_conf_len = sopt.confinement_length >= 0.0 ? sopt.confinement_length : res.mean_thickness;
    const size_t N = m_L.size();

    // grid over the toolpath bounding box
    float xmin = 1e30f, ymin = 1e30f, xmax = -1e30f, ymax = -1e30f, wmax = 0.0f;
    for (const QzSimSegment &s : m_segs) {
        xmin = std::min({xmin, s.x0, s.x1}); xmax = std::max({xmax, s.x0, s.x1});
        ymin = std::min({ymin, s.y0, s.y1}); ymax = std::max({ymax, s.y0, s.y1});
        wmax = std::max(wmax, s.w);
    }
    if (!(xmax > xmin) || !(ymax > ymin)) return false;
    m_cell = std::max(0.25, opt.cell_mm);
    auto size_grid = [&]() {
        const double margin = 2.0 * m_cell + wmax;
        m_x0 = xmin - margin; m_y0 = ymin - margin;
        m_nx = std::max(1, (int) std::ceil((xmax + margin - m_x0) / m_cell));
        m_ny = std::max(1, (int) std::ceil((ymax + margin - m_y0) / m_cell));
    };
    size_grid();
    while ((size_t) m_nx * m_ny * N > opt.max_table) { m_cell *= 1.5; size_grid(); }
    const size_t nc = (size_t) m_nx * m_ny;

    // segments per layer, midpoint cells
    m_layer_segs.assign(N, {});
    m_seg_cell_mid.resize(m_segs.size());
    for (size_t i = 0; i < m_segs.size(); ++i) {
        const QzSimSegment &s = m_segs[i];
        if (s.layer >= 0 && (size_t) s.layer < N) m_layer_segs[s.layer].push_back((int) i);
        m_seg_cell_mid[i] = cell_of(0.5f * (s.x0 + s.x1), 0.5f * (s.y0 + s.y1));
    }

    // nominal height per step, and coverage per layer
    m_H.assign(N * nc, 0.0f);
    std::vector<std::vector<int>> coverage(N);
    std::vector<int> cells;
    for (size_t k = 0; k < N; ++k) {
        float *Hk = &m_H[k * nc];
        if (k > 0) std::copy(&m_H[(k - 1) * nc], &m_H[(k - 1) * nc] + nc, Hk);
        for (int si : m_layer_segs[k]) {
            const QzSimSegment &s = m_segs[si];
            cells.clear();
            cells_along(s.x0, s.y0, s.x1, s.y1, s.w, cells);
            for (int c : cells) { Hk[c] = std::max(Hk[c], s.z); coverage[k].push_back(c); }
        }
        std::sort(coverage[k].begin(), coverage[k].end());
        coverage[k].erase(std::unique(coverage[k].begin(), coverage[k].end()), coverage[k].end());
    }

    // peak utilization table (memory of the plastic squash). Between two steps that raise
    // the column above a cell the load is constant and the strength only grows, so the
    // peak can only sit at a step where the local height field increased.
    // The strength of layer i at step k does not depend on the cell: hoist it out of the
    // cell loop (this table is the expensive part of the build for large jobs).
    m_Upeak.assign(N * nc, 0.0f);
    m_kpeak.assign(N * nc, -1);
    std::vector<float> inv_strength(N, 0.0f);
    const double kappa = 1.0 / (1.0 - m_mat.nu);
    for (size_t i = 0; i < N; ++i) {
        if (coverage[i].empty()) continue;
        const double conf  = (m_conf && m_conf_len > 0.0) ? 1.0 + (kappa - 1.0) * std::exp(-m_L[i].z_bottom / m_conf_len) : 1.0;
        const double z_top = (m_L[i].z_bottom + m_L[i].height) * 1e3;
        for (size_t k = i + 1; k < N; ++k) {
            const double strength = m_mat.sigma_p(m_L[k].t_end - m_L[i].t_start) * conf;
            inv_strength[k] = strength > 0.0 ? (float) (m_mat.rho * QZ_G * 1e-3 / strength) : 1e9f;
        }
        for (int c : coverage[i]) {
            float best = 0.0f; int kb = -1;
            for (size_t k = i + 1; k < N; ++k) {
                const float H = m_H[k * nc + c];
                if (H <= m_H[(k - 1) * nc + c] || (double) H <= z_top) continue;
                const float U = (float) ((double) H - z_top) * inv_strength[k];
                if (U > best) { best = U; kb = (int) k; }
            }
            m_Upeak[i * nc + c] = best;
            m_kpeak[i * nc + c] = kb;
        }
    }

    // sway per step (mm), monotone
    m_sway.assign(N, 0.0f);
    const double z0 = m_L.front().z_bottom;
    for (size_t k = 0; k < N; ++k) {
        const double H   = (m_L[k].z_bottom + m_L[k].height - z0) * 1e3;
        const double lam = (k < lambda_by_top.size()) ? lambda_by_top[k] : 1e9;
        const double d0  = opt.imperfection * H;
        double amp = (lam > 1.0001) ? d0 / (1.0 - 1.0 / lam) : opt.sway_cap * H;
        amp = std::min(amp, opt.sway_cap * H);
        if (k > 0) amp = std::max(amp, (double) m_sway[k - 1]);
        m_sway[k] = (float) amp;
    }

    // collapse: plastic (model) vs buckling (first lambda <= 1), whichever first
    m_k_collapse = -1; m_t_collapse = 0.0; m_hinge = -1; m_by_buckling = false;
    int kb = -1;
    for (size_t k = 0; k < N && k < lambda_by_top.size(); ++k) if (lambda_by_top[k] <= 1.0) { kb = (int) k; break; }
    if (res.collapse_after_layer >= 0) { m_k_collapse = res.collapse_after_layer; m_t_collapse = res.collapse_time; m_hinge = std::max(0, res.critical_layer); }
    if (kb >= 0 && (m_k_collapse < 0 || m_L[kb].t_end < m_t_collapse)) { m_k_collapse = kb; m_t_collapse = m_L[kb].t_end; m_hinge = 0; m_by_buckling = true; }
    // a result computed from another layer list must not index past our tables
    m_k_collapse = std::min(m_k_collapse, (int) N - 1);
    m_hinge      = std::min(m_hinge, std::max(0, m_k_collapse));

    double tl = 0.0;
    for (const QzLayerRecord &l : m_L) tl += std::max(0.0, l.t_end - l.t_start);
    m_mean_layer_time = std::max(1e-3, tl / (double) N);

    // fold direction: weak axis of the hinge layer (or of the last layer when no collapse is
    // predicted), fixed for the whole simulation so the fold never swings as layers are added
    {
        const int ref = m_hinge >= 0 ? m_hinge : (int) N - 1;
        double dx = m_G[ref].dir_x, dy = m_G[ref].dir_y;
        const double dn = std::hypot(dx, dy);
        if (dn < 1e-9) { dx = 1.0; dy = 0.0; } else { dx /= dn; dy /= dn; }
        m_dir_x = dx; m_dir_y = dy;
        // section diameter across the fold direction, from the hinge layer's material
        double dmin = 1e30, dmax = -1e30;
        for (int si : m_layer_segs[ref]) {
            const QzSimSegment &sg = m_segs[si];
            for (const float *q : { &sg.x0, &sg.x1 }) {
                const double d = (q[0] - m_G[ref].cx * 1e3) * dx + (q[1] - m_G[ref].cy * 1e3) * dy;
                dmin = std::min(dmin, d); dmax = std::max(dmax, d);
            }
        }
        m_diam = (dmax > dmin) ? (dmax - dmin) + m_L[ref].thickness * 1e3 : 2.0 * m_G[ref].r_max * 1e3;
        m_diam = std::max(m_diam, 2.0 * m_L[ref].thickness * 1e3);
    }
    m_valid = true;
    return true;
}

void QzStackSim::point_prefold(const QzSimFrame &fr, int layer, int cell, float x, float y, float z_top, float own_h,
                               float &ox, float &oy, float &oz) const
{
    const size_t nc = (size_t) m_nx * m_ny;
    const float eps = fr.squash[(size_t) layer * nc + cell];
    const float S   = fr.settle[(size_t) layer * nc + cell];
    // vertical: settle on the squashed layers below, minus the own squash
    oz = z_top - S - own_h * eps;
    // plan: bulge about the layer centroid. When the stack is heading for a fold the side it
    // will fold towards (the compressed side of the future hinge) bulges more, and the
    // asymmetry is localised around the hinge zone - the one-sided barrel of IMG_4657
    const float cx = (float) (m_G[layer].cx * 1e3), cy = (float) (m_G[layer].cy * 1e3);
    float asym = 0.0f;
    if (fr.phase != QzSimPhase::Stable && fr.hinge_len > 0.0) {
        const float rx = x - cx, ry = y - cy;
        const float rn = std::hypot(rx, ry);
        const float cosphi = rn > 1e-6f ? (float) ((rx * fr.dir_x + ry * fr.dir_y) / rn) : 0.0f;
        const double dz = (z_top - fr.hinge_zc) / std::max(1e-6, fr.hinge_len);
        const double w_h = std::exp(-dz * dz);
        const double ramp = std::min(1.0, std::max(0.0, (fr.r_hinge - m_opt.pre_r0) / std::max(1e-6, 1.0 - m_opt.pre_r0)));
        asym = (float) (m_opt.bulge_asym * w_h * ramp) * std::max(0.0f, cosphi);
    }
    const float sc = 1.0f + (float) m_opt.bulge_gain * eps * (1.0f + asym);
    ox = cx + (x - cx) * sc;
    oy = cy + (y - cy) * sc;
    // sway: clamped-free mode shape on the nominal height
    const double z0  = m_L.front().z_bottom * 1e3;
    const double Ht  = (m_L[fr.top].z_bottom + m_L[fr.top].height) * 1e3 - z0;
    const double phi = Ht > 1e-6 ? (1.0 - std::cos(0.5 * QZ_PI * std::min(1.0, ((double) z_top - z0) / Ht))) : 0.0;
    ox += (float) (fr.sway * phi * fr.dir_x);
    oy += (float) (fr.sway * phi * fr.dir_y);
}

void QzStackSim::bend(const QzSimFrame &fr, double theta, float &x, float &y, float &z) const
{
    if (theta <= 1e-6 || fr.hinge_len <= 0.0) return;
    const double s = (double) z - fr.hinge_z0;
    if (s <= 0.0) return;                                   // standing base
    // local offsets: d along the fold direction, (ex, ey) across it
    const double rx = x - fr.axis_x, ry = y - fr.axis_y;
    double d = rx * fr.dir_x + ry * fr.dir_y;
    const double ex = rx - d * fr.dir_x, ey = ry - d * fr.dir_y;
    const double L = fr.hinge_len, kappa = theta / L, R = 1.0 / kappa;
    // the inner side cannot pass through the bend axis: accordion instead of inversion
    if (d > 0.0) d = std::min(d, m_opt.inner_clamp * R);
    double ad, az, phi;
    if (s <= L) { phi = kappa * s; ad = R * (1.0 - std::cos(phi)); az = fr.hinge_z0 + R * std::sin(phi); }
    else        { phi = theta; ad = R * (1.0 - std::cos(theta)) + (s - L) * std::sin(theta); az = fr.hinge_z0 + R * std::sin(theta) + (s - L) * std::cos(theta); }
    const double pd = ad + d * std::cos(phi), pz = az - d * std::sin(phi);
    x = (float) (fr.axis_x + pd * fr.dir_x + ex);
    y = (float) (fr.axis_y + pd * fr.dir_y + ey);
    z = (float) pz;
}

double QzStackSim::lowest_point(const QzSimFrame &fr, double theta) const
{
    // probe: the endpoints of the top layer's strands (the far end of the rotated part) -
    // the lowest bead bottom for this bend
    const size_t nc = (size_t) m_nx * m_ny;
    double lowest = 1e30;
    const int top = std::min(fr.top, std::max(0, fr.k_collapse));
    for (int j : { top, std::max(0, top - 1) }) {
        for (int si : m_layer_segs[j]) {
            const QzSimSegment &sg = m_segs[si];
            const float pts[2][2] = { { sg.x0, sg.y0 }, { sg.x1, sg.y1 } };
            for (const float *q : pts) {
                const int c = cell_of(q[0], q[1]);
                float ox, oy, oz;
                point_prefold(fr, j, c, q[0], q[1], sg.z, sg.h, ox, oy, oz);
                bend(fr, theta, ox, oy, oz);
                lowest = std::min(lowest, (double) oz - sg.h * (1.0 - fr.squash[(size_t) j * nc + c]));
            }
        }
    }
    return lowest;
}

void QzStackSim::ground(const QzSimFrame &fr, float h, float &x, float &y, float &z, float &w_scale) const
{
    if (!fr.contact) return;
    const float floor_z = 0.5f * h;                     // bead centre cannot go below half a height
    if (z >= floor_z) return;
    const float pen = floor_z - z;
    // the part that hits the bed squashes against it and spreads outwards along the fold
    z = floor_z;
    const float k = (float) fr.settle_f;
    x += (float) (fr.dir_x * pen * 0.6 * k);
    y += (float) (fr.dir_y * pen * 0.6 * k);
    w_scale *= 1.0f + std::min(1.0f, pen / std::max(1e-3f, h)) * 0.5f * k;
}

void QzStackSim::transform_point(const QzSimFrame &fr, int layer, int cell, float x, float y, float z_top, float own_h,
                                 float &ox, float &oy, float &oz, float &w_scale) const
{
    point_prefold(fr, layer, cell, x, y, z_top, own_h, ox, oy, oz);
    bend(fr, fr.fold_angle, ox, oy, oz);
    ground(fr, own_h, ox, oy, oz, w_scale);
}

void QzStackSim::frame(int top, double time, QzSimFrame &fr) const
{
    fr.valid = false;
    if (!m_valid) return;
    const int N = (int) m_L.size();
    top = std::max(0, std::min(N - 1, top));
    const size_t nc = (size_t) m_nx * m_ny;
    fr.top = top; fr.time = time;
    fr.k_hi = top; fr.k_lo = std::max(0, top - 1);
    const double dur = m_L[top].t_end - m_L[top].t_start;
    fr.f = dur > 1e-9 ? (float) std::max(0.0, std::min(1.0, (time - m_L[top].t_start) / dur)) : 1.0f;
    if (top == 0) fr.k_lo = 0;

    fr.collapsed   = m_k_collapse >= 0 && top >= m_k_collapse && time >= m_t_collapse;
    fr.by_buckling = m_by_buckling;
    fr.hinge_layer = m_hinge;
    fr.k_collapse  = m_k_collapse;
    fr.t_collapse  = m_t_collapse;
    fr.sway        = (double) m_sway[fr.k_lo] + fr.f * ((double) m_sway[fr.k_hi] - (double) m_sway[fr.k_lo]);
    fr.dir_x = m_dir_x; fr.dir_y = m_dir_y;
    fr.contact = false; fr.settle_f = 0.0; fr.fold_angle = 0.0; fr.phase = QzSimPhase::Stable;
    fr.hinge_z0 = fr.hinge_len = fr.hinge_zc = 0.0;
    fr.axis_x = m_G[top].cx * 1e3; fr.axis_y = m_G[top].cy * 1e3;

    // squash and settlement tables for layers 0..top
    const size_t rows = (size_t) top + 1;
    fr.squash.assign(rows * nc, 0.0f);
    fr.settle.assign(rows * nc, 0.0f);
    for (int j = 0; j <= top; ++j) {
        float *sq = &fr.squash[(size_t) j * nc];
        for (size_t c = 0; c < nc; ++c) {
            if (m_kpeak[(size_t) j * nc + c] < 0) continue; // never covered by this layer
            const float u = U_peak(j, (int) c, fr.k_lo) + fr.f * (U_peak(j, (int) c, fr.k_hi) - U_peak(j, (int) c, fr.k_lo));
            sq[c] = squash_of(u);
        }
        // (extra crush of the hinge zone is applied after the fold angle is known, below)
        if (j > 0) {
            const float  h_below = (float) (m_L[j - 1].height * 1e3);
            const float *sq_b    = &fr.squash[(size_t) (j - 1) * nc];
            const float *st_b    = &fr.settle[(size_t) (j - 1) * nc];
            float       *st      = &fr.settle[(size_t) j * nc];
            for (size_t c = 0; c < nc; ++c) st[c] = st_b[c] + h_below * sq_b[c];
        }
    }

    // ---- phase machine: Stable -> PreFailure (hinge zone loading up) -> Failure (fold
    //      growing) -> Collapsed (on the bed, settling) -> PostCollapse (still printing)
    if (m_hinge >= 0 && m_k_collapse >= 0) {
        const int h = std::min(m_hinge, top);
        // remembered load/strength of the hinge layer's material at this instant
        double r = 0.0;
        for (int si : m_layer_segs[h]) {
            const int c = m_seg_cell_mid[si];
            const float lo = U_peak(h, c, fr.k_lo), hi = U_peak(h, c, fr.k_hi);
            r = std::max(r, (double) (lo + fr.f * (hi - lo)));
        }
        fr.r_hinge = r;
        // bent zone: centred on the hinge layer (deformed height), length ~ one section diameter,
        // never below the first layer; for a buckling collapse the hinge sits at the base
        const double zc_nom = (m_L[h].z_bottom + 0.5 * m_L[h].height) * 1e3;
        const double zc = zc_nom - (double) layer_mean_settle(fr, h);
        const double L  = m_opt.hinge_len_diam * m_diam;
        double z0 = zc - m_opt.hinge_below * L;
        const double zbase = m_L.front().z_bottom * 1e3;
        if (m_by_buckling || z0 < zbase) z0 = zbase;
        fr.hinge_len = L; fr.hinge_z0 = z0; fr.hinge_zc = z0 + 0.5 * L;
        fr.axis_x = m_G[h].cx * 1e3; fr.axis_y = m_G[h].cy * 1e3;

        const double lean0 = m_opt.lean0_deg * QZ_PI / 180.0;
        if (!fr.collapsed) {
            const double x = (fr.r_hinge - m_opt.pre_r0) / std::max(1e-6, 1.0 - m_opt.pre_r0);
            if (x > 0.0) {
                fr.phase = QzSimPhase::PreFailure;
                const double u = std::min(1.0, x);
                fr.fold_angle = lean0 * u * u * (3.0 - 2.0 * u);           // smooth ramp to the seed lean
            }
        } else {
            // exponential growth from the seed lean until the bed stops it
            const double dt = time - m_t_collapse;
            double theta = lean0 * std::exp(dt / std::max(1e-3, m_opt.fold_tau));
            const double theta_max = m_opt.fold_angle_max * QZ_PI / 180.0;
            theta = std::min(theta, theta_max);
            // contact angle: first theta at which the lowest bead bottom reaches the bed
            double theta_c = theta_max;
            if (lowest_point(fr, theta_max) <= 0.0) {
                double lo = 0.0, hi = theta_max;
                for (int it = 0; it < 28; ++it) { const double mid = 0.5 * (lo + hi); if (lowest_point(fr, mid) <= 0.0) hi = mid; else lo = mid; }
                theta_c = hi;
            }
            if (theta < theta_c) {
                fr.phase = QzSimPhase::Failure;
                fr.fold_angle = theta;
            } else {
                fr.contact = true;
                const double t_contact = m_t_collapse + std::max(1e-3, m_opt.fold_tau) * std::log(std::max(1.0, theta_c / lean0));
                fr.settle_f = std::min(1.0, std::max(0.0, (time - t_contact) / std::max(1e-3, m_opt.settle_time)));
                fr.fold_angle = theta_c + (m_opt.settle_extra_deg * QZ_PI / 180.0) * fr.settle_f;
                fr.phase = (top > m_k_collapse) ? QzSimPhase::PostCollapse : QzSimPhase::Collapsed;
            }
            // the hinge zone crushes as the fold develops (the accordion of the inner side)
            const double fold_f = std::min(1.0, fr.fold_angle / (0.5 * QZ_PI));
            for (int j = 0; j <= top && j <= m_k_collapse; ++j) {
                const double zj = (m_L[j].z_bottom + 0.5 * m_L[j].height) * 1e3;
                const double wz = std::max(0.0, 1.0 - std::fabs(zj - fr.hinge_zc) / std::max(1e-6, 0.5 * L));
                if (wz <= 0.0) continue;
                float *sq = &fr.squash[(size_t) j * nc];
                for (size_t c = 0; c < nc; ++c)
                    if (m_kpeak[(size_t) j * nc + c] >= 0) sq[c] = std::min(0.9f, sq[c] + (float) (m_opt.hinge_squash * fold_f * wz));
            }
            // and the settlement below is recomputed with the crushed hinge
            for (int j = 1; j <= top; ++j) {
                const float  h_below = (float) (m_L[j - 1].height * 1e3);
                const float *sq_b = &fr.squash[(size_t) (j - 1) * nc];
                const float *st_b = &fr.settle[(size_t) (j - 1) * nc];
                float       *st   = &fr.settle[(size_t) j * nc];
                for (size_t c = 0; c < nc; ++c) st[c] = st_b[c] + h_below * sq_b[c];
            }
        }
    }

    // deformed height of the top layer's material (a ring has no material at its centroid)
    fr.height_deformed = layer_mean_top(fr, top) - m_L.front().z_bottom * 1e3;

    // landing field and fallen strands (deposited after the collapse)
    if (fr.collapsed) {
        const bool rebuild_land = fr.land_top != top || std::fabs(fr.land_fold_angle - (fr.fold_angle + fr.settle_f)) > 0.5 * QZ_PI / 180.0 ||
                                  fr.land.size() != nc || fr.fallen_idx.size() != m_segs.size();
        if (rebuild_land) build_landing(fr);
    } else {
        fr.land_top = -1; fr.land_fold_angle = -1.0;
        fr.fallen_a.clear(); fr.fallen_b.clear(); fr.fallen_idx.clear();
    }
    fr.fallen_count = fr.fallen_a.size();

    // current max utilization over the deposited segments (for the card); util() needs a
    // valid frame, and everything it reads is in place by now
    fr.valid = true;
    fr.max_util = 0.0; fr.max_ratio = 0.0;
    for (int j = 0; j <= top; ++j)
        for (int si : m_layer_segs[j]) {
            fr.max_util  = std::max(fr.max_util, (double) util(fr, si));
            fr.max_ratio = std::max(fr.max_ratio, (double) ratio(fr, si));
        }
}

float QzStackSim::layer_mean_settle(const QzSimFrame &fr, int layer) const
{
    const size_t nc = (size_t) m_nx * m_ny;
    if (m_layer_segs[layer].empty()) return 0.0f;
    double sum = 0.0;
    for (int si : m_layer_segs[layer]) sum += fr.settle[(size_t) layer * nc + m_seg_cell_mid[si]];
    return (float) (sum / (double) m_layer_segs[layer].size());
}

double QzStackSim::layer_mean_top(const QzSimFrame &fr, int layer) const
{
    const size_t nc = (size_t) m_nx * m_ny;
    const double z_top = (m_L[layer].z_bottom + m_L[layer].height) * 1e3;
    if (m_layer_segs[layer].empty()) return z_top;
    double sum = 0.0;
    for (int si : m_layer_segs[layer]) {
        const int c = m_seg_cell_mid[si];
        sum += z_top - fr.settle[(size_t) layer * nc + c] - m_L[layer].height * 1e3 * fr.squash[(size_t) layer * nc + c];
    }
    return sum / (double) m_layer_segs[layer].size();
}

bool QzStackSim::settled_landing(std::vector<float> &land, double &t_settled) const
{
    land.clear();
    if (!m_valid || m_k_collapse < 0) return false;
    const double lean0 = m_opt.lean0_deg * QZ_PI / 180.0;
    const double theta_max = m_opt.fold_angle_max * QZ_PI / 180.0;
    // the latest the fold can take + the settling time
    t_settled = m_t_collapse + std::max(1e-3, m_opt.fold_tau) * std::log(std::max(1.0, theta_max / lean0)) + m_opt.settle_time + 0.1;
    QzSimFrame fr;
    frame(m_k_collapse, t_settled, fr);
    if (!fr.valid) return false;
    build_landing(fr, true);
    land = fr.land;
    return true;
}

void QzStackSim::build_landing(QzSimFrame &fr, bool standing_only) const
{
    const size_t nc  = (size_t) m_nx * m_ny;
    const int    top = fr.top;
    // footprint radius (cells) of a bead of width w
    auto radius = [this](float w) { return std::max(0, (int) std::ceil(0.5 * w / m_cell - 0.5)); };
    // stamp a strand into `field`: its top z interpolated along the strand, over its footprint
    // (rc = 0: the cells on the strand line only)
    auto stamp = [&](std::vector<float> &field, const QzSimPoint &a, const QzSimPoint &b, int rc) {
        const float len = std::hypot(b.x - a.x, b.y - a.y);
        const int   ns  = std::max(1, (int) std::ceil(len / (0.5f * (float) m_cell)));
        for (int s = 0; s <= ns; ++s) {
            const float t = (float) s / (float) ns;
            const float z = a.z + t * (b.z - a.z);
            const int ix = (int) std::floor((a.x + t * (b.x - a.x) - m_x0) / m_cell);
            const int iy = (int) std::floor((a.y + t * (b.y - a.y) - m_y0) / m_cell);
            for (int dy = -rc; dy <= rc; ++dy)
                for (int dx = -rc; dx <= rc; ++dx) {
                    const int cx = ix + dx, cy = iy + dy;
                    if (cx < 0 || cy < 0 || cx >= m_nx || cy >= m_ny) continue;
                    float &f = field[(size_t) cy * m_nx + cx];
                    f = std::max(f, z);
                }
        }
    };
    // height of `field` at the cell of a bead end. Deliberately the own cell only: reading
    // a neighbourhood makes the highest point of the pile creep outwards layer after layer
    // until fallen strands hover in the air with nothing under them.
    auto land_at = [&](const std::vector<float> &field, float x, float y) { return field[cell_of(x, y)]; };

    // 1) what is left standing: layers deposited up to the collapse, deformed and folded
    fr.land.assign(nc, 0.0f);
    for (int j = 0; j <= std::min(top, m_k_collapse); ++j)
        for (int si : m_layer_segs[j]) {
            const QzSimSegment &s = m_segs[si];
            const int ca = cell_of(s.x0, s.y0), cb = cell_of(s.x1, s.y1);
            QzSimPoint a, b; float wsa = 1.0f, wsb = 1.0f;
            transform_point(fr, j, ca, s.x0, s.y0, s.z, s.h, a.x, a.y, a.z, wsa);
            transform_point(fr, j, cb, s.x1, s.y1, s.z, s.h, b.x, b.y, b.z, wsb);
            stamp(fr.land, a, b, radius(s.w * (1.0f + fr.squash[(size_t) j * nc + ca]) * std::max(wsa, wsb)));
        }
    fr.land_top = top; fr.land_fold_angle = fr.fold_angle + fr.settle_f;
    if (standing_only) { fr.fallen_a.clear(); fr.fallen_b.clear(); fr.fallen_idx.assign(m_segs.size(), -1); return; }

    // 2) strands deposited after the collapse. Each layer lands on the field as it was when
    //    the layer started (its own strands lie side by side, they do not stack on each
    //    other) and then raises it. Each end of a strand finds its own landing height at its
    //    own cell, so a strand reaching the edge of the pile drapes down to the ground
    //    instead of hovering at the level of the highest point it touches, and the pile is
    //    stamped along the strand line only: the heap grows under the toolpath and spreads
    //    slowly along it (a fraction of a strand per layer) rather than lifting the whole
    //    path into the air. No strand lands above the nozzle.
    fr.fallen_a.clear(); fr.fallen_b.clear();
    fr.fallen_idx.assign(m_segs.size(), -1);
    std::vector<float> land_prev;
    for (int j = m_k_collapse + 1; j <= top; ++j) {
        if (m_layer_segs[j].empty()) continue;
        land_prev = fr.land;
        for (int si : m_layer_segs[j]) {
            const QzSimSegment &s = m_segs[si];
            // jitter keyed by the endpoint itself (not the segment) so the shared endpoint of
            // two consecutive strands lands in the same place and the bead stays continuous
            auto jitter = [&](float x, float y, float &jx, float &jy) {
                const uint32_t hsh = hash_u32(((uint32_t) (int32_t) std::lround(x * 10.0f) * 73856093u) ^
                                              ((uint32_t) (int32_t) std::lround(y * 10.0f) * 19349663u) ^
                                              ((uint32_t) s.layer * 83492791u) ^ m_opt.seed);
                jx = ((float) (hsh & 0xFFFF) / 65535.0f - 0.5f) * (float) m_opt.drop_jitter * s.w;
                jy = ((float) ((hsh >> 16) & 0xFFFF) / 65535.0f - 0.5f) * (float) m_opt.drop_jitter * s.w;
            };
            float jax, jay, jbx, jby;
            jitter(s.x0, s.y0, jax, jay); jitter(s.x1, s.y1, jbx, jby);
            QzSimPoint a{ s.x0 + jax, s.y0 + jay, 0.0f }, b{ s.x1 + jbx, s.y1 + jby, 0.0f };
            const float hf = s.h / (float) m_opt.drop_flatten;
            a.z = std::min(s.z, land_at(land_prev, a.x, a.y) + hf);
            b.z = std::min(s.z, land_at(land_prev, b.x, b.y) + hf);
            stamp(fr.land, a, b, 0);
            fr.fallen_idx[si] = (int) fr.fallen_a.size();
            fr.fallen_a.push_back(a); fr.fallen_b.push_back(b);
        }
    }
}

float QzStackSim::util(const QzSimFrame &fr, int seg) const
{
    if (!fr.valid || seg < 0 || (size_t) seg >= m_segs.size()) return 0.0f;
    const QzSimSegment &s = m_segs[seg];
    if (s.layer < 0 || s.layer > fr.top) return 0.0f;
    const int c = m_seg_cell_mid[seg];
    const float lo = U_inst(s.layer, c, fr.k_lo), hi = U_inst(s.layer, c, fr.k_hi);
    return lo + fr.f * (hi - lo);
}

float QzStackSim::ratio(const QzSimFrame &fr, int seg) const
{
    if (!fr.valid || seg < 0 || (size_t) seg >= m_segs.size()) return 0.0f;
    const QzSimSegment &s = m_segs[seg];
    if (s.layer < 0 || s.layer > fr.top) return 0.0f;
    const int c = m_seg_cell_mid[seg];
    const float lo = U_peak(s.layer, c, fr.k_lo), hi = U_peak(s.layer, c, fr.k_hi);
    return lo + fr.f * (hi - lo);
}

void QzStackSim::deform(const QzSimFrame &fr, int seg, QzSimPoint &a, QzSimPoint &b,
                        float &w_scale, float &h_scale, bool &fallen) const
{
    w_scale = 1.0f; h_scale = 1.0f; fallen = false;
    if (seg < 0 || (size_t) seg >= m_segs.size()) { a = QzSimPoint(); b = QzSimPoint(); return; }
    const QzSimSegment &s = m_segs[seg];
    a = { s.x0, s.y0, s.z }; b = { s.x1, s.y1, s.z };
    if (!fr.valid || s.layer < 0 || s.layer > fr.top) return;
    const size_t nc = (size_t) m_nx * m_ny;
    if (fr.collapsed && s.layer > m_k_collapse) {
        const int idx = ((size_t) seg < fr.fallen_idx.size()) ? fr.fallen_idx[seg] : -1;
        if (idx >= 0 && (size_t) idx < fr.fallen_a.size()) {
            a = fr.fallen_a[idx]; b = fr.fallen_b[idx];
            w_scale = (float) m_opt.drop_flatten; h_scale = 1.0f / (float) m_opt.drop_flatten;
            fallen = true;
        }
        return;
    }
    const int ca = cell_of(s.x0, s.y0), cb = cell_of(s.x1, s.y1);
    float wsa = 1.0f, wsb = 1.0f;
    transform_point(fr, s.layer, ca, s.x0, s.y0, s.z, s.h, a.x, a.y, a.z, wsa);
    transform_point(fr, s.layer, cb, s.x1, s.y1, s.z, s.h, b.x, b.y, b.z, wsb);
    const float eps = 0.5f * (fr.squash[(size_t) s.layer * nc + ca] + fr.squash[(size_t) s.layer * nc + cb]);
    w_scale = (1.0f + eps) * 0.5f * (wsa + wsb); h_scale = 1.0f - eps;
}

}} // namespace Slic3r::QuasiZero
