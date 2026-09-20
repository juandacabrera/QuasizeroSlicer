// Quasizero Slicer - PRO extension through the external qz-sim engine. GNU AGPLv3, part of
// the Quasizero fork of OrcaSlicer.
#include "QzProClient.hpp"

#include "slic3r/GUI/GCodeViewer.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/Plater.hpp"
#include "slic3r/GUI/Camera.hpp"
#include "slic3r/GUI/GLShader.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/OpenGLManager.hpp"
#include "libslic3r/AppConfig.hpp"

#include <imgui/imgui.h>
#include <wx/filedlg.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>

#include <algorithm>
#include <cmath>

namespace Slic3r {
namespace GUI {

// the LITE edition provides the client as the PRO extension: with no engine installed it is
// inert apart from the activation rows of the Stability card
std::unique_ptr<QzProHooks> qz_make_pro_hooks(GCodeViewer &viewer) { return std::make_unique<QzProClient>(viewer); }

static const char *QZ_ENGINE_PATH_KEY = "qz_engine_path";

QzProClient::~QzProClient() { stop_engine(); }

// ---- engine lifecycle ------------------------------------------------------------------

bool QzProClient::ensure_engine()
{
    if (m_client.connected() && m_status.running) return m_status.licensed;
    m_engine_tried = true;
    m_status = QzEngineStatus();
    const std::string configured = wxGetApp().app_config->has(QZ_ENGINE_PATH_KEY) ? wxGetApp().app_config->get(QZ_ENGINE_PATH_KEY) : std::string();
    const std::string slicer_dir = wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath().ToUTF8().data();
    m_status.exe_path = qz_find_engine(configured, slicer_dir);
    m_status.found = !m_status.exe_path.empty();
    if (!m_status.found) { m_status.error = _u8L("Quasizero PRO engine (qz-sim) not found"); return false; }
    auto process = std::make_unique<QzEngineProcess>();
    std::string err;
    if (!process->start(m_status.exe_path, {}, err)) { m_status.error = err; return false; }
    m_client.set_transport(std::move(process));
    if (!m_client.hello(m_status)) { m_client.set_transport(nullptr); m_status.running = false; return false; }
    return m_status.licensed;
}

void QzProClient::stop_engine()
{
    if (m_client.connected()) m_client.quit();
    m_client.set_transport(nullptr);
    m_status.running = false;
    m_job_loaded = false;
}

void QzProClient::set_engine_path(const std::string &path)
{
    wxGetApp().app_config->set(QZ_ENGINE_PATH_KEY, path);
    stop_engine();
    if (ensure_engine()) build_job();
}

bool QzProClient::install_licence(const std::string &path)
{
    if (!m_client.connected() && !ensure_engine() && !m_status.running) return false;
    const bool ok = m_client.licence(path, true, m_status);
    if (ok && m_status.licensed && !m_job_loaded) build_job();
    return ok && m_status.licensed;
}

// ---- hooks -----------------------------------------------------------------------------

bool QzProClient::deform_active() const { return m_deform_view && m_job_loaded && m_load.sim_valid; }

void QzProClient::reset()
{
    if (m_job_loaded && m_client.connected()) m_client.unload();
    m_job_loaded = false;
    m_load = QzEngineLoadInfo();
    m_frame = QzEngineFrame();
    m_seg_of_vertex.clear();
    m_skeleton = QuasiZero::QzSkeleton();
    m_tube_geos.clear();
    m_frame_error.clear();
    m_cache_vertex = size_t(-1); m_cache_time = -1.0;
    m_playing = false; m_play_transient = false;
    for (GLModel &gm : m_models) gm.reset();
}

void QzProClient::on_loaded()
{
    // start the engine only when the job would use it (a characterised material with layers)
    const GCodeViewer::QzStability &st = m_v.m_qz_stability;
    if (!st.result.valid || st.geom.empty()) return;
    if (!m_engine_tried || (m_status.found && !m_status.running)) ensure_engine();
    if (m_status.licensed) build_job();
}

void QzProClient::build_job()
{
    const GCodeViewer::QzStability &st = m_v.m_qz_stability;
    libvgcode::Viewer &viewer = m_v.m_viewer;
    m_seg_of_vertex.clear();
    m_job_loaded = false;
    m_load = QzEngineLoadInfo();
    m_frame = QzEngineFrame();
    m_cache_vertex = size_t(-1);
    for (GLModel &gm : m_models) gm.reset();
    if (!st.result.valid || st.geom.empty() || !m_client.connected()) return;
    const size_t n = viewer.get_vertices_count();
    if (n == 0) return;
    m_seg_of_vertex.assign(n, -1);
    std::vector<QuasiZero::QzSimSegment> segs;
    segs.reserve(n / 2);
    // running process time: Viewer::get_estimated_time_at() accumulates from vertex 0 on
    // every call, which would make this loop quadratic
    const size_t tmode = static_cast<size_t>(viewer.get_time_mode());
    float t_acc = viewer.get_vertex_at(0).times[tmode];
    for (size_t i = 1; i < n; ++i) {
        const libvgcode::PathVertex &v = viewer.get_vertex_at(i);
        t_acc += v.times[tmode];
        if (!v.is_extrusion() || v.layer_id >= st.layer_index_of_id.size()) continue;
        const int li = st.layer_index_of_id[v.layer_id];
        if (li < 0) continue;
        const libvgcode::PathVertex &u = viewer.get_vertex_at(i - 1);
        const float dx = v.position[0] - u.position[0], dy = v.position[1] - u.position[1];
        if (dx * dx + dy * dy < 1e-8f) continue;
        QuasiZero::QzSimSegment s;
        s.x0 = u.position[0]; s.y0 = u.position[1]; s.x1 = v.position[0]; s.y1 = v.position[1];
        s.z = v.position[2]; s.w = v.width; s.h = v.height; s.layer = li;
        s.t_end = t_acc;
        m_seg_of_vertex[i] = (int) segs.size();
        segs.push_back(s);
    }
    if (segs.empty()) { m_seg_of_vertex.clear(); return; }
    m_skeleton.build(segs);
    const nlohmann::json job = qz_engine_job(st.material, st.options, st.layers, st.geom, segs);
    if (!m_client.load(job, m_load)) {
        m_status.error = m_load.error;
        if (!m_client.connected()) m_status.running = false;
        m_seg_of_vertex.clear(); m_skeleton = QuasiZero::QzSkeleton();
        return;
    }
    if (m_load.nodes != m_skeleton.nodes().size()) {
        // the engine and the slicer disagree on the toolpath skeleton: different versions
        m_status.error = _u8L("The engine and the slicer build different toolpath skeletons (versions do not match)");
        m_load.sim_valid = false;
        return;
    }
    m_job_loaded = true;
}

void QzProClient::rebuild_deformed_mesh()
{
    libvgcode::Viewer &viewer = m_v.m_viewer;
    if (!m_job_loaded || !m_load.sim_valid) return;
    const libvgcode::Interval &vis = viewer.get_view_visible_range();
    const libvgcode::Interval &full = viewer.get_view_full_range();
    const size_t nverts = viewer.get_vertices_count();
    if (nverts == 0 || vis[1] >= nverts || m_seg_of_vertex.size() != nverts) return;
    // top layer = record of the last visible extrusion vertex
    int top = -1;
    for (size_t i = vis[1] + 1; i-- > 0;) {
        const int sg = m_seg_of_vertex[i];
        if (sg >= 0) { top = m_v.m_qz_stability.layer_index_of_id[viewer.get_vertex_at(i).layer_id]; break; }
        if (vis[1] - i > 20000) break;
    }
    const double t = (double) viewer.get_estimated_time_at(vis[1]);
    m_cache_vertex = vis[1];
    m_cache_time   = t;
    // visible segments: from the first extrusion of the layers range to the player position
    const size_t start = std::max<size_t>(full[0], 1);
    int seg_lo = -1, seg_hi = -1;
    for (size_t i = start; i <= vis[1]; ++i) if (m_seg_of_vertex[i] >= 0) { seg_lo = m_seg_of_vertex[i]; break; }
    for (size_t i = vis[1] + 1; i-- > start;) if (m_seg_of_vertex[i] >= 0) { seg_hi = m_seg_of_vertex[i]; break; }
    if (top < 0 || seg_lo < 0 || seg_hi < seg_lo) {
        for (GLModel &gm : m_models) gm.reset();
        m_frame.valid = false;
        return;
    }
    std::string err;
    if (!m_client.frame(top, t, seg_lo, seg_hi, m_frame, err)) {
        m_frame_error = err;
        if (!m_client.connected()) { m_status.running = false; m_status.error = err; m_job_loaded = false; }
        for (GLModel &gm : m_models) gm.reset();
        return;
    }
    m_frame_error.clear();
    if (!m_frame.valid || m_frame.pose.size() != m_skeleton.nodes().size()) {
        for (GLModel &gm : m_models) gm.reset();
        return;
    }
    // colours: the Stability legend palette, binned by the load/strength each strand remembers
    const libvgcode::ColorRange &range = viewer.get_color_range(libvgcode::EViewType::Stability);
    for (size_t b = 0; b < BINS; ++b) {
        const libvgcode::Color c = range.get_color_at((float(b) + 0.5f) / float(BINS));
        m_colors[b] = ColorRGBA(float(c[0]) / 255.0f, float(c[1]) / 255.0f, float(c[2]) / 255.0f, 1.0f);
    }
    QuasiZero::QzTubeOptions topt;
    topt.sides = (seg_hi - seg_lo > 40000) ? 4 : 8;
    topt.bins  = (int) BINS;
    QuasiZero::QzTubeMesher::mesh(m_skeleton, m_frame.pose, topt, m_tube_geos);
    for (const std::vector<QuasiZero::QzSimPoint> &chain : m_frame.chains) {
        if (chain.size() < 2) continue;
        // unloaded material: bottom of the load/strength ladder
        QuasiZero::QzTubeMesher::mesh_polyline(chain, m_frame.bead_w, m_frame.bead_h, 0.02f, topt, m_tube_geos);
    }
    for (size_t b = 0; b < BINS; ++b) {
        m_models[b].reset();
        if (b >= m_tube_geos.size()) continue;
        QuasiZero::QzTubeGeometry &tg = m_tube_geos[b];
        if (tg.vertices_count() == 0 || tg.indices.empty()) continue;
        GLModel::Geometry g;
        g.format = { GLModel::Geometry::EPrimitiveType::Triangles, GLModel::Geometry::EVertexLayout::P3N3 };
        g.vertices.resize(tg.positions.size() * 2);
        for (size_t v = 0; v < tg.vertices_count(); ++v) {
            g.vertices[6 * v + 0] = tg.positions[3 * v + 0]; g.vertices[6 * v + 1] = tg.positions[3 * v + 1]; g.vertices[6 * v + 2] = tg.positions[3 * v + 2];
            g.vertices[6 * v + 3] = tg.normals[3 * v + 0];   g.vertices[6 * v + 4] = tg.normals[3 * v + 1];   g.vertices[6 * v + 5] = tg.normals[3 * v + 2];
        }
        g.indices.assign(tg.indices.begin(), tg.indices.end());
        m_models[b].init_from(std::move(g));
        m_models[b].set_color(m_colors[b]);
    }
}

void QzProClient::render_deformed()
{
    libvgcode::Viewer &viewer = m_v.m_viewer;
    const libvgcode::Interval &vis = viewer.get_view_visible_range();
    const double t = (viewer.get_vertices_count() > vis[1]) ? (double) viewer.get_estimated_time_at(vis[1]) : 0.0;
    // a layer change in the play leaves the visible range at the END of the new layer for a
    // frame or two: keep the last mesh instead of flashing the state of the end of the layer
    const bool transient = m_play_transient ||
        (m_playing && m_cache_vertex != size_t(-1) && vis[1] > m_cache_vertex && t - m_cache_time > 10.0);
    if (!transient && (vis[1] != m_cache_vertex || std::fabs(t - m_cache_time) > 0.25))
        rebuild_deformed_mesh();

    GLShaderProgram *shader = wxGetApp().get_shader("gouraud_light");
    if (shader == nullptr) return;
    shader->start_using();
    shader->set_uniform("emission_factor", 0.0f);
    const Camera &camera = wxGetApp().plater()->get_camera();
    const Transform3d &view_matrix = camera.get_view_matrix();
    shader->set_uniform("view_model_matrix", view_matrix);
    shader->set_uniform("projection_matrix", camera.get_projection_matrix());
    const Matrix3d view_normal_matrix = view_matrix.matrix().block(0, 0, 3, 3);
    shader->set_uniform("view_normal_matrix", view_normal_matrix);
    for (GLModel &gm : m_models)
        if (gm.is_initialized()) gm.render();
    shader->stop_using();
}

double QzProClient::play_rate(double rate)
{
    if (!deform_active() || m_load.collapse_step < 0) return rate;
    libvgcode::Viewer &viewer = m_v.m_viewer;
    const libvgcode::Interval &vis = viewer.get_view_visible_range();
    const double t_now = (viewer.get_vertices_count() > vis[1]) ? (double) viewer.get_estimated_time_at(vis[1]) : -1.0;
    const double t_c   = m_load.collapse_time;
    if (t_now >= t_c - 0.5 && t_now <= t_c + 3.5) return std::max(1.0, rate / 10.0);
    return rate;
}

// ---- activation UI ---------------------------------------------------------------------

void QzProClient::request_locate_engine()
{
    wxGetApp().CallAfter([this]() {
        wxFileDialog dlg(wxGetApp().plater(), _L("Locate the Quasizero PRO engine"), wxEmptyString, qz_engine_exe_name(),
#ifdef _WIN32
                         "qz-sim.exe|qz-sim.exe|" + _L("All files") + " (*.*)|*.*",
#else
                         "qz-sim|qz-sim|" + _L("All files") + " (*)|*",
#endif
                         wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dlg.ShowModal() == wxID_OK) set_engine_path(dlg.GetPath().ToUTF8().data());
    });
}

void QzProClient::request_activate_licence()
{
    wxGetApp().CallAfter([this]() {
        wxFileDialog dlg(wxGetApp().plater(), _L("Select your Quasizero PRO licence file"), wxEmptyString, wxEmptyString,
                         _L("Quasizero licence") + " (*.qzl)|*.qzl|" + _L("All files") + " (*.*)|*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dlg.ShowModal() == wxID_OK) install_licence(dlg.GetPath().ToUTF8().data());
    });
}

void QzProClient::render_card(const QzProCardContext &ctx)
{
    ImGui::Dummy(ImVec2(0.0f, 2.0f * ctx.scale));
    // engine / licence status rows and the activation buttons
    if (!m_engine_tried) ensure_engine();
    const QzEngineStatus &s = m_status;
    if (!s.found) {
        ctx.kv(_u8L("Quasizero PRO").c_str(), _u8L("engine not installed"), ctx.label);
        ImGui::SetWindowFontScale(0.85f);
        ImGui::PushTextWrapPos(330.0f * ctx.scale);
        ImGui::TextColored(*ctx.label, "%s", _u8L("The deformation view (collapse kinematics and the bead after the collapse) is computed by the Quasizero PRO engine, a separate program with its own licence. Install the PRO add-on or point the slicer at qz-sim.").c_str());
        ImGui::PopTextWrapPos();
        ImGui::SetWindowFontScale(1.0f);
        if (ImGui::Button((_u8L("Locate qz-sim...") + "##qzloc").c_str())) request_locate_engine();
        ImGui::SameLine();
        if (ImGui::Button((_u8L("Check again") + "##qzchk").c_str())) { stop_engine(); m_engine_tried = false; }
        return;
    }
    if (!s.running) {
        ctx.kv(_u8L("Quasizero PRO").c_str(), s.error.empty() ? _u8L("engine not running") : s.error, ctx.bad);
        if (ImGui::Button((_u8L("Restart engine") + "##qzrst").c_str())) { stop_engine(); if (ensure_engine()) build_job(); }
        ImGui::SameLine();
        if (ImGui::Button((_u8L("Locate qz-sim...") + "##qzloc").c_str())) request_locate_engine();
        return;
    }
    if (!s.licensed) {
        std::string why;
        if (!s.licence_signed && s.licence_error.empty()) why = _u8L("no licence installed");
        else if (s.licence_expired) why = _u8L("licence expired on") + " " + s.expires;
        else if (s.licence_signed) why = _u8L("licence does not include the PRO features");
        else why = s.licence_error;
        ctx.kv(_u8L("Quasizero PRO").c_str(), _u8L("engine") + " " + s.version + ", " + why, ctx.warn);
        if (ImGui::Button((_u8L("Activate licence...") + "##qzact").c_str())) request_activate_licence();
        ImGui::SameLine();
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextColored(*ctx.label, "%s", _u8L("(.qzl file from Quasizero)").c_str());
        ImGui::SetWindowFontScale(1.0f);
        return;
    }
    ctx.kv(_u8L("Quasizero PRO").c_str(), (s.licensee.empty() ? _u8L("licensed") : s.licensee) + (s.expires.empty() ? std::string() : ", " + _u8L("until") + " " + s.expires) + "  (" + _u8L("engine") + " " + s.version + ")", ctx.ok);
    if (!m_status.error.empty() && !m_job_loaded) { ctx.kv(_u8L("Engine").c_str(), m_status.error, ctx.bad); return; }

    bool deform = m_deform_view;
    if (ImGui::Checkbox((_u8L("Show deformation") + "##qzdef").c_str(), &deform)) {
        m_deform_view = deform;
        m_cache_vertex = size_t(-1);
    }
    if (!deform) {
        ImGui::SetWindowFontScale(0.85f);
        ImGui::PushTextWrapPos(330.0f * ctx.scale);
        ImGui::TextColored(*ctx.label, "%s", _u8L("Level 0 model: yield + structuration, bed confinement. Calibrate the material with the collapse tests.").c_str());
        ImGui::PopTextWrapPos();
        ImGui::SetWindowFontScale(1.0f);
        return;
    }
    if (!m_job_loaded || !m_load.sim_valid) {
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextColored(*ctx.label, "%s", (m_frame_error.empty() ? _u8L("No toolpath segments to simulate.") : m_frame_error).c_str());
        ImGui::SetWindowFontScale(1.0f);
    } else if (m_frame.valid) {
        const QzEngineFrame &fs = m_frame;
        const char *phase_txt = fs.phase == "pre_failure" ? "Pre-failure" : fs.phase == "failure" ? "Failure" :
                                fs.phase == "collapsed" ? "Collapsed" : fs.phase == "post_collapse" ? "Post-collapse" : "Stable";
        const ImVec4 *pc = fs.phase == "stable" ? ctx.ok : (fs.phase == "pre_failure" ? ctx.warn : ctx.bad);
        ctx.kv(_u8L("Phase").c_str(), _u8L(phase_txt), pc);
        if (fs.collapsed) {
            ctx.kv(_u8L("Fold").c_str(), (fs.by_buckling ? _u8L("buckling") : _u8L("plastic hinge")) + " - " + _u8L("layer") + " " + std::to_string(fs.hinge_layer + 1) + ", " + ctx.fmt("%.0f", fs.fold_angle_deg) + " deg" + (fs.contact ? ", " + _u8L("on the bed") : std::string()), ctx.bad);
            if (fs.has_bead)
                ctx.kv(_u8L("Falling").c_str(), ctx.fmt("%.0f mm", fs.strand_mm) + " " + _u8L("of strand") + ", " + ctx.fmt("%.0f mm", fs.strand_at_rest_mm) + " " + _u8L("at rest on the pile"), ctx.bad);
            else if (fs.fallen_count > 0)
                ctx.kv(_u8L("Falling").c_str(), std::to_string(fs.fallen_count) + " " + _u8L("strands on the pile"), ctx.bad);
        } else {
            if (fs.phase == "pre_failure")
                ctx.kv(_u8L("Hinge zone").c_str(), ctx.fmt("%.0f %%", 100.0 * fs.r_hinge) + "  " + _u8L("load/strength") + ", " + _u8L("lean") + " " + ctx.fmt("%.1f", fs.fold_angle_deg) + " deg", ctx.warn);
            const ImVec4 *uc = fs.max_ratio >= 1.0 ? ctx.bad : (fs.max_ratio > ctx.sf_target ? ctx.warn : ctx.ok);
            ctx.kv(_u8L("Now").c_str(), ctx.fmt("%.1f mm", fs.height_deformed) + " " + _u8L("tall") + ", " + _u8L("sway") + " " + ctx.fmt("%.1f mm", fs.sway), ctx.ok);
            ctx.kv(_u8L("Colour, max").c_str(), ctx.fmt("%.0f %%", 100.0 * fs.max_ratio) + "  " + _u8L("load/strength so far"), uc);
        }
    }
    ImGui::SetWindowFontScale(0.85f);
    ImGui::PushTextWrapPos(330.0f * ctx.scale);
    ImGui::TextColored(*ctx.label, "%s", _u8L("Kinematic view driven by the model: every layer printed so far stays visible, each strand keeps the peak load/strength it has seen (colour and squash), the hinge zone bulges towards the fold side, then bends over a distributed hinge until it meets the bed and settles; the bead extruded afterwards hangs from the head, falls and piles up on what is left. Collapse plays in slow motion. Not a nonlinear FEM.").c_str());
    ImGui::PopTextWrapPos();
    ImGui::SetWindowFontScale(1.0f);
}

} // namespace GUI
} // namespace Slic3r
