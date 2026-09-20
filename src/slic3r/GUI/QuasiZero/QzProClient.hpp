// Quasizero Slicer - the PRO extension of the preview when the simulation engine is the
// external `qz-sim` process: the slicer sends the job, the engine answers frames (node poses
// and the post-collapse bead), the slicer re-skins and draws them. Also the activation UI
// (locate the engine, install a licence file). GNU AGPLv3, part of the Quasizero fork of
// OrcaSlicer. Contains no engine code: it speaks the documented protocol only.
#pragma once

#include "QzProHooks.hpp"

#include "slic3r/GUI/GLModel.hpp"
#include "slic3r/Utils/QzEngine.hpp"
#include "libslic3r/Color.hpp"
#include "libslic3r/QuasiZero/QzSkeleton.hpp"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace Slic3r {
namespace GUI {

class QzProClient : public QzProHooks
{
public:
    explicit QzProClient(GCodeViewer &viewer) : m_v(viewer) {}
    ~QzProClient() override;

    void   on_loaded() override;
    void   reset() override;
    bool   deform_active() const override;
    void   render_deformed() override;
    double play_rate(double rate) override;
    void   on_play_layer_change() override { m_play_transient = true; }
    void   on_play_frame(bool playing) override { m_playing = playing; m_play_transient = false; }
    void   render_card(const QzProCardContext &ctx) override;

    // engine management (also used by the activation UI)
    bool ensure_engine();                       // find + start + hello; false with m_status.error
    void stop_engine();
    void set_engine_path(const std::string &path);   // user picked an executable
    bool install_licence(const std::string &path);   // user picked a .qzl file
    const QzEngineStatus &status() const { return m_status; }

private:
    void build_job();
    void rebuild_deformed_mesh();
    void request_locate_engine();
    void request_activate_licence();

    GCodeViewer &m_v;
    static constexpr size_t BINS = 16;
    std::array<GLModel, BINS>   m_models;
    std::array<ColorRGBA, BINS> m_colors;

    QzEngineClient                   m_client;         // owns the engine process (its transport)
    QzEngineStatus                   m_status;
    bool                             m_engine_tried = false;

    QzEngineLoadInfo                       m_load;           // the job the engine holds
    bool                                   m_job_loaded = false;
    std::vector<int>                       m_seg_of_vertex;  // libvgcode vertex id -> segment (-1 = none)
    QuasiZero::QzSkeleton                  m_skeleton;       // built here from the same segments (node ids match the engine's)
    QzEngineFrame                          m_frame;          // last answered frame
    std::vector<QuasiZero::QzTubeGeometry> m_tube_geos;
    std::string                            m_frame_error;

    bool   m_deform_view = false;
    bool   m_playing = false;
    bool   m_play_transient = false;
    size_t m_cache_vertex = size_t(-1);
    double m_cache_time = -1.0;
};

} // namespace GUI
} // namespace Slic3r
