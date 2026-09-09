// Quasizero Slicer - paste stability model (buildability / buckling during printing).
// GNU AGPLv3, part of the Quasizero fork of OrcaSlicer.
//
// Physics (see docs/qzmini/stability.md):
//  * plastic collapse, layer by layer, with time-dependent strength
//      sigma_j = rho*g*(z_top - z_j)          load of everything above layer j
//      sigma_p = k_p * tau0(age),  tau0(t) = tau0_0 + A_thix*t      [Roussel 2018, Perrot 2016]
//      base confinement 1/(1-nu) decaying with height                [Suiker 2018 eq. 79-80]
//    U_j = sigma_j / sigma_p ; collapse when max_j U_j >= 1
//  * elastic buckling of a free straight wall growing at rate ldot with age-dependent
//    stiffness E(t) = E0 (1 + xi_E t), solved by Rayleigh-Ritz (heavy column, clamped-free);
//    no-curing limit l_cr = (7.837 D0 / (rho g h))^(1/3)             [Suiker 2018]
//  * closed cylinder: local axial shell buckling with a knockdown factor  [Timoshenko]
//
// Pure C++17, no libslic3r/Eigen dependencies: usable from the GUI, from the G-code
// pipeline and from the standalone test runner.
#pragma once

#include <cstddef>
#include <functional>
#include <vector>

namespace Slic3r { namespace QuasiZero {

constexpr double QZ_G = 9.81;

struct QzPasteMaterial
{
    double rho   = 1500.0;             // wet density [kg/m^3]
    double tau0  = 0.0;                // static yield stress at deposition [Pa]; 0 = not characterised
    double athix = 0.0;                // linear structuration rate [Pa/s]
    double E0    = 30000.0;            // initial elastic modulus [Pa]
    double xiE   = 0.0;                // linear stiffening rate [1/s]  E(t) = E0 (1 + xiE t)
    double nu    = 0.3;                // Poisson ratio
    double kp    = 1.7320508075688772; // sigma_p = kp * tau0  (sqrt(3): von Mises, 2: Tresca)

    bool   characterised() const { return tau0 > 0.0 && rho > 0.0; }
    double tau(double age_s) const;
    double sigma_p(double age_s) const;
    double E(double age_s) const;
};

struct QzLayerRecord
{
    double z_bottom  = 0.0; // [m]
    double height    = 0.0; // [m]
    double t_start   = 0.0; // [s] deposition start (process time)
    double t_end     = 0.0; // [s] layer completed
    double thickness = 0.0; // [m] mean bead width
};

struct QzStabilityOptions
{
    bool   base_confinement   = true;
    double confinement_length = -1.0; // [m]; < 0 -> mean bead width
    double safety_factor      = 1.5;
};

struct QzStabilityResult
{
    bool valid = false;
    // per layer j: max over the print of U_j (what the colour map shows)
    std::vector<float> peak_utilization;
    // history[k][j]: U of layer j right after layer k completes (j <= k)
    std::vector<std::vector<float>> history;
    int    collapse_after_layer = -1;  // k (0-based) when max U first reaches 1; -1 = never
    int    critical_layer       = -1;  // j (0-based) that yields; -1 = none
    double collapse_time        = 0.0; // [s]
    double collapse_height      = 0.0; // [m]
    double max_utilization      = 0.0;
    double height_total         = 0.0; // [m]
    double build_rate           = 0.0; // [m/s] mean vertical growth
    double mean_thickness       = 0.0; // [m]
    double max_height_plastic   = 0.0; // [m] closed form (linear curing); <0 = unbounded
    double critical_build_rate  = 0.0; // [m/s]
    double buckling_height_wall = 0.0; // [m] free straight wall, single bead, no curing
    double buckling_height_wall_cured = 0.0; // [m] same with age-dependent stiffness
    double min_time_scale       = 1.0; // multiply all layer times by this to keep U <= 1/SF; <0 = impossible
};

// --- core -----------------------------------------------------------------------------
QzStabilityResult qz_evaluate_stability(const QzPasteMaterial &mat, const std::vector<QzLayerRecord> &layers,
                                        const QzStabilityOptions &opt);

// Per-layer utilization field as it stands right after step k, with memory: for every
// layer j <= k the peak of U_j over the steps j..k (monotone in k; at the last step it
// equals `peak_utilization`). This is what a display of "the state at layer k" shows.
std::vector<float> qz_utilization_upto(const QzStabilityResult &res, int k);

// --- closed forms and helpers ----------------------------------------------------------
double qz_max_height_plastic_linear(const QzPasteMaterial &mat, double build_rate_m_s);   // [m], <0 unbounded
double qz_critical_build_rate(const QzPasteMaterial &mat);                                 // [m/s]
double qz_heavy_column_load_factor(const std::function<double(double)> &EI_of_x, double q, double L,
                                   int n_terms = 8, int n_int = 1000);
double qz_buckling_height_free_wall_closed_form(const QzPasteMaterial &mat, double thickness_m);
double qz_buckling_height_free_wall(const QzPasteMaterial &mat, double thickness_m, double build_rate_m_s,
                                    double corrugation_amp_m = 0.0, double H_max_m = 2.0);
double qz_cylinder_shell_buckling_height(const QzPasteMaterial &mat, double thickness_m, double radius_m,
                                         double knockdown = 0.3);
// two identical cylinders printed at two vertical rates, collapsing at H1 and H2 -> (tau0, athix)
bool qz_identify_two_cylinder_tests(double rho, double kp, double H1_m, double rate1_m_s, double H2_m,
                                    double rate2_m_s, double &tau0_out, double &athix_out);

// --- layer records from a move sequence (any source) -----------------------------------
struct QzMoveSample
{
    float    z_top     = 0.0f; // [mm] print z of the move end
    float    height    = 0.0f; // [mm]
    float    width     = 0.0f; // [mm]
    float    dt        = 0.0f; // [s] duration of this move (dwells included)
    unsigned layer_id  = 0;
    bool     extrusion = false;
};
// returns layers sorted by z; `layer_index_of_id` maps a move layer_id to the record index (-1 if none)
std::vector<QzLayerRecord> qz_layers_from_moves(const std::vector<QzMoveSample> &moves,
                                                std::vector<int> &layer_index_of_id);

// --- Level 1.5: deformation kinematics driven by the model ----------------------------
// Per-layer plan geometry of the toolpath (from the extrusion segments of the layer)
struct QzLayerGeom
{
    double cx = 0.0, cy = 0.0;     // [m] centroid of the deposited material
    double area = 0.0;             // [m^2] plan area of material (sum length*width)
    double I_min = 0.0;            // [m^4] minimum second moment of area of the layer section
    double dir_x = 1.0, dir_y = 0.0; // unit direction of the weak (buckling) axis
    double r_max = 0.0;            // [m] max distance centroid -> material
};

struct QzDeformOptions
{
    double U_yield          = 0.5;   // squash starts above this utilization           [hyp]
    double eps_max          = 0.35;  // squash strain at U = 1                          [hyp]
    double bulge_gain       = 0.6;   // plan scale = 1 + gain * squash                  [hyp]
    double imperfection     = 0.002; // initial sway / height                           [hyp: Suiker uses deflections ~ bead]
    double sway_cap         = 0.25;  // max sway / height before it counts as buckled
    double fold_angle_max   = 75.0;  // [deg]
    double fold_time_layers = 2.0;   // fold completes in this many mean layer times
    double hinge_squash     = 0.45;  // extra squash of the hinge layer
};

struct QzDeformLayer
{
    double squash = 0.0;                 // strain 0..1
    double z_bottom = 0.0, height = 0.0; // [m] deformed
    double scale_xy = 1.0;               // plan scale about the centroid (bulge)
    double sway_x = 0.0, sway_y = 0.0;   // [m]
    double width_scale = 1.0;            // bead width factor
    float  util = 0.0f;                  // U at this time
};

struct QzDeformState
{
    bool   valid = false;
    int    top = -1;             // last deposited layer index
    double time = 0.0;           // [s]
    std::vector<QzDeformLayer> layers;   // 0..top
    double lambda_cr = 1e9;      // buckling load factor of the current stack
    double sway_amp = 0.0;       // [m] sway at the top
    double dir_x = 1.0, dir_y = 0.0;
    int    hinge_layer = -1;     // >= 0 once collapsed
    bool   collapsed = false;
    bool   by_buckling = false;
    double fold_angle = 0.0;     // [rad]
    double pivot_x = 0.0, pivot_y = 0.0, pivot_z = 0.0; // [m]
    double height_deformed = 0.0;
};

// buckling load factor of the stack 0..k for every k (time = t_end of layer k)
std::vector<double> qz_buckling_load_factors(const QzPasteMaterial &mat, const std::vector<QzLayerRecord> &layers,
                                             const std::vector<QzLayerGeom> &geom);
// full kinematic state of the stack at (top, time)
QzDeformState qz_deformation_state(const QzPasteMaterial &mat, const std::vector<QzLayerRecord> &layers,
                                   const std::vector<QzLayerGeom> &geom, const QzStabilityResult &res,
                                   const std::vector<double> &lambda_by_top, int top, double time,
                                   const QzDeformOptions &opt);
// map an undeformed toolpath point (m, z = bead top) of layer `layer` to its deformed position
void qz_deform_point(const QzDeformState &st, const std::vector<QzLayerRecord> &layers,
                     const std::vector<QzLayerGeom> &geom, int layer,
                     double x, double y, double z, double &ox, double &oy, double &oz);

}} // namespace Slic3r::QuasiZero
