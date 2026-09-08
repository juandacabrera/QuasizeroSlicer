// Quasizero Slicer - paste stability model. GNU AGPLv3.
#include "QzStabilityModel.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace Slic3r { namespace QuasiZero {

// ------------------------------------------------------------------ material
double QzPasteMaterial::tau(double age_s) const
{
    return tau0 + athix * std::max(0.0, age_s);
}
double QzPasteMaterial::sigma_p(double age_s) const { return kp * tau(age_s); }
double QzPasteMaterial::E(double age_s) const { return E0 * (1.0 + xiE * std::max(0.0, age_s)); }

// ------------------------------------------------------------------ closed forms
double qz_max_height_plastic_linear(const QzPasteMaterial &mat, double ldot)
{
    if (ldot <= 0.0) return -1.0;
    const double sp0   = mat.kp * mat.tau0;
    const double denom = mat.rho * QZ_G - mat.kp * mat.athix / ldot;
    return denom <= 0.0 ? -1.0 : sp0 / denom;
}

double qz_critical_build_rate(const QzPasteMaterial &mat)
{
    return mat.kp * mat.athix / (mat.rho * QZ_G);
}

// small dense solver (Gaussian elimination with partial pivoting), n <= 16
static bool solve_dense(std::vector<double> A, std::vector<double> b, int n, std::vector<double> &x)
{
    for (int c = 0; c < n; ++c) {
        int piv = c;
        for (int r = c + 1; r < n; ++r)
            if (std::fabs(A[r * n + c]) > std::fabs(A[piv * n + c])) piv = r;
        if (std::fabs(A[piv * n + c]) < 1e-300) return false;
        if (piv != c) {
            for (int k = 0; k < n; ++k) std::swap(A[c * n + k], A[piv * n + k]);
            std::swap(b[c], b[piv]);
        }
        for (int r = c + 1; r < n; ++r) {
            const double f = A[r * n + c] / A[c * n + c];
            if (f == 0.0) continue;
            for (int k = c; k < n; ++k) A[r * n + k] -= f * A[c * n + k];
            b[r] -= f * b[c];
        }
    }
    x.assign(n, 0.0);
    for (int r = n - 1; r >= 0; --r) {
        double s = b[r];
        for (int k = r + 1; k < n; ++k) s -= A[r * n + k] * x[k];
        x[r] = s / A[r * n + r];
    }
    return true;
}

// Rayleigh-Ritz: phi_k = (x/L)^(k+1), k = 1..n  (w(0) = w'(0) = 0, clamped base, free top)
// K_ij = int EI phi_i'' phi_j'' ;  G_ij = int N phi_i' phi_j' ,  N(x) = q (L - x)
// lambda_cr = min eig of K w = lambda G w = 1 / max eig(K^-1 G)   (power iteration)
double qz_heavy_column_load_factor(const std::function<double(double)> &EI_of_x, double q, double L, int n_terms, int n_int)
{
    const int n = std::max(2, std::min(n_terms, 14));
    const int m = std::max(50, n_int);
    std::vector<double> K(n * n, 0.0), G(n * n, 0.0);
    const double dx = L / (m - 1);
    std::vector<double> d1(n), d2(n);
    for (int i = 0; i < m; ++i) {
        const double x  = i * dx;
        const double w  = (i == 0 || i == m - 1) ? 0.5 * dx : dx;
        const double EI = EI_of_x(x);
        const double N  = q * (L - x);
        const double s  = x / L;
        for (int k = 1; k <= n; ++k) {
            d1[k - 1] = (k + 1) * std::pow(s, k) / L;
            d2[k - 1] = (k + 1) * k * std::pow(s, k - 1) / (L * L);
        }
        for (int a = 0; a < n; ++a)
            for (int b = 0; b < n; ++b) {
                K[a * n + b] += EI * d2[a] * d2[b] * w;
                G[a * n + b] += N * d1[a] * d1[b] * w;
            }
    }
    // diagonal scaling for conditioning
    std::vector<double> sc(n);
    for (int a = 0; a < n; ++a) sc[a] = 1.0 / std::sqrt(std::max(K[a * n + a], 1e-300));
    for (int a = 0; a < n; ++a)
        for (int b = 0; b < n; ++b) {
            K[a * n + b] *= sc[a] * sc[b];
            G[a * n + b] *= sc[a] * sc[b];
        }
    // power iteration on K^-1 G
    std::vector<double> v(n, 1.0), y, gv(n);
    double mu = 0.0;
    for (int it = 0; it < 300; ++it) {
        for (int a = 0; a < n; ++a) {
            double s = 0.0;
            for (int b = 0; b < n; ++b) s += G[a * n + b] * v[b];
            gv[a] = s;
        }
        if (!solve_dense(K, gv, n, y)) return std::numeric_limits<double>::infinity();
        double norm = 0.0;
        for (double t : y) norm += t * t;
        norm = std::sqrt(norm);
        if (norm < 1e-300) return std::numeric_limits<double>::infinity();
        const double mu_new = norm; // ||A v|| with ||v|| = 1 approximates the dominant eigenvalue
        for (int a = 0; a < n; ++a) v[a] = y[a] / norm;
        if (it > 5 && std::fabs(mu_new - mu) < 1e-10 * std::max(1.0, mu_new)) { mu = mu_new; break; }
        mu = mu_new;
    }
    return mu > 0.0 ? 1.0 / mu : std::numeric_limits<double>::infinity();
}

static double wall_D(double E, double h, double nu) { return E * h * h * h / (12.0 * (1.0 - nu * nu)); }

double qz_buckling_height_free_wall_closed_form(const QzPasteMaterial &mat, double h)
{
    if (h <= 0.0) return 0.0;
    return std::cbrt(7.837 * wall_D(mat.E0, h, mat.nu) / (mat.rho * QZ_G * h));
}

double qz_buckling_height_free_wall(const QzPasteMaterial &mat, double h, double ldot, double A, double H_max)
{
    if (h <= 0.0 || ldot <= 0.0) return 0.0;
    const double q      = mat.rho * QZ_G * h;
    const double I_geom = h * h * h / 12.0 + h * A * A / 2.0;
    auto lam = [&](double L) {
        auto EI = [&](double x) { return mat.E((L - x) / ldot) * I_geom / (1.0 - mat.nu * mat.nu); };
        return qz_heavy_column_load_factor(EI, q, L);
    };
    double lo = 1e-3, hi = H_max;
    if (lam(hi) > 1.0) return std::numeric_limits<double>::infinity();
    for (int i = 0; i < 40; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (lam(mid) > 1.0) lo = mid; else hi = mid;
    }
    return hi;
}

double qz_cylinder_shell_buckling_height(const QzPasteMaterial &mat, double h, double R, double knockdown)
{
    if (h <= 0.0 || R <= 0.0) return 0.0;
    const double s_cr = knockdown * mat.E0 * h / (R * std::sqrt(3.0 * (1.0 - mat.nu * mat.nu)));
    return s_cr / (mat.rho * QZ_G);
}

bool qz_identify_two_cylinder_tests(double rho, double kp, double H1, double r1, double H2, double r2,
                                    double &tau0, double &athix)
{
    // rho g H_i = sp0 + kp * athix * H_i / r_i
    const double a11 = 1.0, a12 = kp * H1 / r1, a21 = 1.0, a22 = kp * H2 / r2;
    const double det = a11 * a22 - a12 * a21;
    if (std::fabs(det) < 1e-12 || r1 <= 0 || r2 <= 0) return false;
    const double b1 = rho * QZ_G * H1, b2 = rho * QZ_G * H2;
    const double sp0 = (b1 * a22 - a12 * b2) / det;
    athix            = (a11 * b2 - a21 * b1) / det;
    tau0             = sp0 / kp;
    return tau0 > 0.0;
}

// ------------------------------------------------------------------ layers from moves
std::vector<QzLayerRecord> qz_layers_from_moves(const std::vector<QzMoveSample> &moves, std::vector<int> &layer_index_of_id)
{
    struct Acc { double z_top = 0, h_sum = 0, w_sum = 0; size_t n = 0; double t_first = -1, t_last = 0; };
    std::map<unsigned, Acc> acc;
    double t = 0.0;
    unsigned max_id = 0;
    for (const QzMoveSample &m : moves) {
        if (m.layer_id > max_id) max_id = m.layer_id;
        if (m.extrusion) {
            Acc &a = acc[m.layer_id];
            if (a.t_first < 0.0) a.t_first = t;
            a.z_top = std::max(a.z_top, (double) m.z_top);
            a.h_sum += m.height; a.w_sum += m.width; ++a.n;
        }
        t += std::max(0.0f, m.dt);
        auto it = acc.find(m.layer_id);
        if (it != acc.end()) it->second.t_last = t;
    }
    std::vector<std::pair<unsigned, Acc>> v(acc.begin(), acc.end());
    std::sort(v.begin(), v.end(), [](const auto &a, const auto &b) { return a.second.z_top < b.second.z_top; });
    layer_index_of_id.assign(max_id + 1, -1);
    std::vector<QzLayerRecord> out;
    out.reserve(v.size());
    for (size_t i = 0; i < v.size(); ++i) {
        const Acc &a = v[i].second;
        QzLayerRecord r;
        r.height    = a.n ? a.h_sum / a.n * 1e-3 : 0.0;
        r.thickness = a.n ? a.w_sum / a.n * 1e-3 : 0.0;
        r.z_bottom  = a.z_top * 1e-3 - r.height;
        r.t_start   = a.t_first < 0.0 ? 0.0 : a.t_first;
        r.t_end     = std::max(a.t_last, r.t_start);
        layer_index_of_id[v[i].first] = (int) i;
        out.push_back(r);
    }
    return out;
}

// ------------------------------------------------------------------ core evaluation
static void evaluate_plastic(const QzPasteMaterial &mat, const std::vector<QzLayerRecord> &L, const QzStabilityOptions &opt,
                             double time_scale, double conf_len, QzStabilityResult *res, double &max_u, int &collapse_k, int &crit_j)
{
    const size_t n = L.size();
    const double kappa = 1.0 / (1.0 - mat.nu);
    max_u = 0.0; collapse_k = -1; crit_j = -1;
    if (res) { res->history.assign(n, {}); res->peak_utilization.assign(n, 0.0f); }
    for (size_t k = 0; k < n; ++k) {
        const double now   = L[k].t_end * time_scale;
        const double z_top = L[k].z_bottom + L[k].height;
        std::vector<float> row(k + 1, 0.0f);
        for (size_t j = 0; j <= k; ++j) {
            const double stress = mat.rho * QZ_G * std::max(0.0, z_top - L[j].z_bottom - L[j].height);
            double strength     = mat.sigma_p(now - L[j].t_start * time_scale);
            if (opt.base_confinement && conf_len > 0.0)
                strength *= 1.0 + (kappa - 1.0) * std::exp(-L[j].z_bottom / conf_len);
            const double U = strength > 0.0 ? stress / strength : 1e9;
            row[j] = (float) U;
            if (res && U > res->peak_utilization[j]) res->peak_utilization[j] = (float) U;
            if (U > max_u) { max_u = U; }
            if (collapse_k < 0 && U >= 1.0) { collapse_k = (int) k; crit_j = (int) j; }
        }
        if (res) res->history[k] = std::move(row);
    }
    if (crit_j < 0 && res) {
        // no collapse: report the layer with the highest peak utilization
        float best = -1.0f;
        for (size_t j = 0; j < n; ++j) if (res->peak_utilization[j] > best) { best = res->peak_utilization[j]; crit_j = (int) j; }
    }
}

QzStabilityResult qz_evaluate_stability(const QzPasteMaterial &mat, const std::vector<QzLayerRecord> &L, const QzStabilityOptions &opt)
{
    QzStabilityResult r;
    if (!mat.characterised() || L.empty()) return r;
    double th = 0.0; size_t nth = 0;
    for (const QzLayerRecord &l : L) if (l.thickness > 0.0) { th += l.thickness; ++nth; }
    r.mean_thickness = nth ? th / nth : 0.0;
    const double conf_len = opt.confinement_length >= 0.0 ? opt.confinement_length : r.mean_thickness;

    r.height_total = L.back().z_bottom + L.back().height - L.front().z_bottom;
    const double t_total = L.back().t_end - L.front().t_start;
    r.build_rate = t_total > 0.0 ? r.height_total / t_total : 0.0;

    double max_u; int ck, cj;
    evaluate_plastic(mat, L, opt, 1.0, conf_len, &r, max_u, ck, cj);
    r.max_utilization      = max_u;
    r.collapse_after_layer = ck;
    r.critical_layer       = cj;
    if (ck >= 0) {
        r.collapse_time   = L[ck].t_end;
        r.collapse_height = L[ck].z_bottom + L[ck].height - L.front().z_bottom;
    }
    r.max_height_plastic  = qz_max_height_plastic_linear(mat, r.build_rate);
    r.critical_build_rate = qz_critical_build_rate(mat);
    if (r.mean_thickness > 0.0) {
        r.buckling_height_wall = qz_buckling_height_free_wall_closed_form(mat, r.mean_thickness);
        r.buckling_height_wall_cured = r.build_rate > 0.0 ? qz_buckling_height_free_wall(mat, r.mean_thickness, r.build_rate) : r.buckling_height_wall;
    }
    // time scale needed to keep U <= 1/SF (uniform stretch of the whole schedule)
    const double target = 1.0 / std::max(1.0, opt.safety_factor);
    if (max_u <= target) {
        r.min_time_scale = 1.0;
    } else {
        double lo = 1.0, hi = 1.0;
        bool ok = false;
        for (int i = 0; i < 12 && !ok; ++i) {
            hi *= 2.0;
            double mu; int k2, j2;
            evaluate_plastic(mat, L, opt, hi, conf_len, nullptr, mu, k2, j2);
            ok = mu <= target;
        }
        if (!ok) r.min_time_scale = -1.0;
        else {
            for (int i = 0; i < 30; ++i) {
                const double mid = 0.5 * (lo + hi);
                double mu; int k2, j2;
                evaluate_plastic(mat, L, opt, mid, conf_len, nullptr, mu, k2, j2);
                if (mu <= target) hi = mid; else lo = mid;
            }
            r.min_time_scale = hi;
        }
    }
    r.valid = true;
    return r;
}

}} // namespace Slic3r::QuasiZero
