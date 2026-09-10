// Quasizero Slicer - QzStackSim: history-aware, grid-resolved kinematic simulation of a
// paste print (Level 1.5 v2). GNU AGPLv3.
//
// What it adds over the per-layer state of QzStabilityModel:
//  * spatial resolution: a plan grid (~2 mm cells) carries the nominal material height per
//    deposition step, so the load on a point of layer i is the local column above it
//    (irregular geometries load different zones of the same layer differently)
//  * memory: every (layer, cell) remembers the peak load it has seen -> plastic squash is
//    irreversible and accumulates layer after layer
//  * settlement: each point drops by the squash of the material below it (barrel / dents)
//  * collapse: layers deposited before the collapse fold about the hinge; layers deposited
//    after it are NOT transformed - they land at their nominal XY (with a deterministic
//    jitter) on top of the deformed pile (height field), strand after strand
//
// Kinematic visualisation driven by the analytical model - not a nonlinear FEM.
#pragma once

#include "QzStabilityModel.hpp"

#include <cstdint>
#include <vector>

namespace Slic3r { namespace QuasiZero {

struct QzSimSegment
{
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0; // [mm] plan endpoints
    float z = 0;                          // [mm] nominal bead top
    float w = 0, h = 0;                   // [mm] bead width / height
    int   layer = -1;                     // stability record index
    float t_end = 0;                      // [s] process time at the segment end
};

// Every number below is a visual/kinematic hypothesis [hyp], tuned on IMG_4657 (cocoa-paste
// cylinder collapse) - see QZMINI_STABILITY.md section 1c.
struct QzSimOptions
{
    double cell_mm        = 2.0;   // grid cell; enlarged automatically for huge jobs
    // yielding (squash + bulge), driven by the remembered load/strength per cell
    double U_yield        = 0.5;   // squash starts above this utilization
    double eps_max        = 0.35;  // squash strain at U = 1
    double bulge_gain     = 0.6;   // plan scale about the layer centroid = 1 + gain*eps
    double bulge_asym     = 1.2;   // extra bulge on the side the stack will fold towards (compressed side), x this
    double pre_r0         = 0.6;   // pre-failure starts when the hinge zone reaches this load/strength
    // imperfection sway (elastic)
    double imperfection   = 0.002; // initial sway / height
    double sway_cap       = 0.25;  // max sway / height
    // distributed hinge and fold
    double hinge_len_diam = 0.8;   // length of the bent zone, in section diameters (8-10 layers in IMG_4657)
    double hinge_below    = 0.25;  // fraction of the bent zone below the critical layer (the confined, older layers
                                   // under it stay standing; the yielding zone extends mostly upwards)
    double lean0_deg      = 1.5;   // lean reached at the end of the pre-failure phase; seed of the fold
    double fold_tau       = 0.4;   // [s] print time; the fold grows as lean0 * exp((t - t_collapse) / tau)
    double fold_angle_max = 150.0; // [deg] cap when nothing stops the fold (short stub above the hinge)
    double settle_time    = 1.0;   // [s] after bed contact: the arch settles and widens
    double settle_extra_deg = 8.0; // extra rotation absorbed while settling (clamped by the bed)
    double hinge_squash   = 0.45;  // extra squash inside the hinge zone at full fold (inner side)
    double inner_clamp    = 0.8;   // inner-side offset capped at this fraction of the bend radius (accordion, no inversion)
    // fallen strands (height-field sketch until the bead simulator takes over)
    double drop_jitter    = 0.25;  // XY jitter of fallen strands, in bead widths (peak to peak)
    double drop_flatten   = 1.35;  // width factor of fallen strands (height factor = 1/this)
    size_t max_table      = 4000000; // max (layers x cells) before the cell grows
    uint32_t seed         = 0x9E3779B9u;
};

enum class QzSimPhase : int { Stable = 0, PreFailure, Failure, Collapsed, PostCollapse };

struct QzSimPoint { float x = 0, y = 0, z = 0; };

// State for one playback instant. Owned by the caller and reused frame to frame.
struct QzSimFrame
{
    bool   valid = false;
    int    top = -1;              // last deposited layer index
    double time = 0.0;
    int    k_lo = 0, k_hi = 0;    // interpolated deposition steps
    float  f = 0.0f;              // 0..1 progress inside step k_hi
    bool   collapsed = false;     // the fold has started (time >= t_collapse)
    bool   by_buckling = false;
    int    hinge_layer = -1;      // centre of the bent zone (>= 0 once collapsed)
    int    k_collapse = -1;       // step at which collapse happened
    double t_collapse = 0.0;
    QzSimPhase phase = QzSimPhase::Stable;
    double r_hinge = 0.0;         // remembered load/strength of the hinge zone at this instant
    double fold_angle = 0.0;      // [rad] total bend of the axis (pre-failure lean included)
    bool   contact = false;       // the fallen part touches the bed
    double settle_f = 0.0;        // 0..1 settling progress after contact
    double sway = 0.0;            // [mm] at the top
    double dir_x = 1.0, dir_y = 0.0;                    // fold direction (unit, plan)
    double axis_x = 0.0, axis_y = 0.0;                  // [mm] bend axis (hinge layer centroid)
    double hinge_z0 = 0.0, hinge_len = 0.0, hinge_zc = 0.0; // [mm] bent zone [z0, z0+len], centre
    double height_deformed = 0.0; // [mm] mean deformed top of the top layer's material (before any fold)
    double max_util = 0.0;        // current (instantaneous) load/strength, max over the deposited segments
    double max_ratio = 0.0;       // remembered load/strength (peak up to this instant), max over the deposited segments
    size_t fallen_count = 0;      // strands deposited after the collapse in this frame
    // internal caches (sized by the simulator)
    std::vector<float> settle;    // [mm] settlement below layer j at cell c: settle[j*ncells + c]
    std::vector<float> squash;    // own squash of layer j at cell c
    std::vector<float> land;      // [mm] landing height field for fallen strands
    std::vector<QzSimPoint> fallen_a, fallen_b; // per fallen strand, in deposition order
    std::vector<int>   fallen_idx; // per segment: index into fallen_a/b, -1 = not fallen
    double land_fold_angle = -1.0; int land_top = -1;   // cache keys of `land`
};

class QzStackSim
{
public:
    bool build(const QzPasteMaterial &mat, const std::vector<QzLayerRecord> &layers,
               const std::vector<QzLayerGeom> &geom, std::vector<QzSimSegment> segments,
               const QzStabilityResult &res, const std::vector<double> &lambda_by_top,
               const QzStabilityOptions &sopt, const QzSimOptions &opt);
    bool   valid() const { return m_valid; }
    size_t segments_count() const { return m_segs.size(); }
    const QzSimSegment &segment(size_t i) const { return m_segs[i]; }
    int    cells_x() const { return m_nx; }
    int    cells_y() const { return m_ny; }
    double cell_mm() const { return m_cell; }
    double grid_x0() const { return m_x0; }
    double grid_y0() const { return m_y0; }
    // height field (per cell, [mm] bead top) of what is left standing once the collapse has
    // settled - the ground the post-collapse bead falls on. False when no collapse is predicted.
    bool   settled_landing(std::vector<float> &land, double &t_settled) const;
    int    collapse_step() const { return m_k_collapse; }
    double collapse_time() const { return m_t_collapse; }
    int    hinge_layer() const { return m_hinge; }
    bool   collapse_by_buckling() const { return m_by_buckling; }

    // compute the state for the playback position (top = last deposited layer index)
    void frame(int top, double time, QzSimFrame &fr) const;
    // current (instantaneous) local utilization of a segment in this frame
    float util(const QzSimFrame &fr, int seg) const;
    // remembered local utilization: the peak the segment's material has seen up to this
    // frame (monotone along the play, never resets when the print moves on) - the colour
    // of the deformed view
    float ratio(const QzSimFrame &fr, int seg) const;
    // deformed endpoints of a segment; `fallen` = dropped strand deposited after the collapse
    void deform(const QzSimFrame &fr, int seg, QzSimPoint &a, QzSimPoint &b,
                float &w_scale, float &h_scale, bool &fallen) const;

private:
    int  cell_of(float x, float y) const;
    void cells_along(float x0, float y0, float x1, float y1, float w, std::vector<int> &out) const;
    float U_inst(int layer, int cell, int k) const;   // instantaneous, step k
    float U_peak(int layer, int cell, int k) const;   // peak up to step k
    float squash_of(float U) const;
    // yielding: settlement on the squashed layers below, own squash, (asymmetric) bulge, sway
    void  point_prefold(const QzSimFrame &fr, int layer, int cell, float x, float y, float z_top, float own_h,
                        float &ox, float &oy, float &oz) const;
    // distributed hinge: bend the axis with constant curvature over the hinge zone, rigid
    // continuation above; inner side compresses (accordion), outer side opens
    void  bend(const QzSimFrame &fr, double theta, float &x, float &y, float &z) const;
    // bed contact of the part above the hinge for a given bend; returns the lowest bead bottom
    double lowest_point(const QzSimFrame &fr, double theta) const;
    // after contact: nothing below the bed, the arch settles and widens
    void  ground(const QzSimFrame &fr, float h, float &x, float &y, float &z, float &w_scale) const;
    // everything a pre-collapse point goes through (prefold + bend + ground)
    void  transform_point(const QzSimFrame &fr, int layer, int cell, float x, float y, float z_top, float own_h,
                          float &ox, float &oy, float &oz, float &w_scale) const;
    // deformed top z (before any fold) averaged over the material of a layer (segment midpoints)
    double layer_mean_top(const QzSimFrame &fr, int layer) const;
    float  layer_mean_settle(const QzSimFrame &fr, int layer) const;
    void  build_landing(QzSimFrame &fr, bool standing_only = false) const;
    static uint32_t hash_u32(uint32_t v);

    bool m_valid = false;
    QzPasteMaterial m_mat;
    std::vector<QzLayerRecord> m_L;
    std::vector<QzLayerGeom>   m_G;
    std::vector<QzSimSegment>  m_segs;
    std::vector<std::vector<int>> m_layer_segs;   // segment indices per layer (deposition order)
    QzSimOptions m_opt;
    double m_conf_len = 0.0;                      // [m] bed confinement length
    bool   m_conf = true;
    // grid
    double m_x0 = 0, m_y0 = 0, m_cell = 2.0; int m_nx = 0, m_ny = 0;
    // per step k: nominal height (mm) per cell (0 = nothing)
    std::vector<float> m_H;
    // per layer i per cell: peak U over the whole print and the step where it occurs (-1 = never loaded)
    std::vector<float> m_Upeak;
    std::vector<int>   m_kpeak;
    // per step: sway amplitude (mm), monotone
    std::vector<float> m_sway;
    // fold direction (fixed for the whole simulation) and section diameter across it
    double m_dir_x = 1.0, m_dir_y = 0.0, m_diam = 0.0;
    std::vector<int>   m_seg_cell_mid;            // cell of each segment midpoint
    // collapse
    int    m_k_collapse = -1; double m_t_collapse = 0.0; int m_hinge = -1; bool m_by_buckling = false;
    double m_mean_layer_time = 1.0;
};

}} // namespace Slic3r::QuasiZero
