// Quasizero Slicer - hooks for the PRO extension of the preview (deformation view, collapse
// kinematics, post-collapse bead). GNU AGPLv3, part of the Quasizero fork of OrcaSlicer.
//
// The LITE edition ships this interface and a stub factory that returns nothing: the
// preview then shows the Stability panel (Level 0/1) only. The PRO edition provides an
// implementation (QzProView, built with QZ_PRO=ON against the Quasizero simulation engine)
// through the same factory. GCodeViewer and GLCanvas3D only ever talk to this interface, so
// the two editions differ by one source file and one CMake option.
#pragma once

#include <functional>
#include <memory>
#include <string>

struct ImVec4;

namespace Slic3r {
namespace GUI {

class GCodeViewer;

// what the Stability card hands to the PRO rows: the row printer, the palette and the
// safety-margin target it uses itself
struct QzProCardContext
{
    std::function<void(const char *label, const std::string &value, const ImVec4 *color)> kv;
    std::function<std::string(const char *fmt, double v)> fmt;
    const ImVec4 *ok = nullptr, *warn = nullptr, *bad = nullptr, *label = nullptr;
    double sf_target = 1.0;
    float  scale = 1.0f;
};

class QzProHooks
{
public:
    virtual ~QzProHooks() = default;

    // the viewer has loaded a job and evaluated its stability (Level 0/1): build whatever
    // the extension needs from the viewer's vertices
    virtual void on_loaded() = 0;
    // the viewer is being reset: drop every derived state (user toggles survive)
    virtual void reset() = 0;
    // true when the extension draws the print in place of the nominal toolpaths
    virtual bool deform_active() const = 0;
    // draw the extension's view of the print (called after the nominal toolpaths were masked)
    virtual void render_deformed() = 0;
    // the play loop: rate = moves per second the player is about to use; the extension may
    // return a different one (slow motion around an event)
    virtual double play_rate(double rate) = 0;
    // the play loop moved to the next layer this frame (the sliders settle over the next frames)
    virtual void on_play_layer_change() = 0;
    // called every frame of the moves capsule with the play state
    virtual void on_play_frame(bool playing) = 0;
    // after the play loop advanced: progress towards the next vertex, 0..1 (0 when paused)
    virtual void on_play_fraction(double fraction) = 0;
    // the extension's rows of the Stability card (after the Level 0/1 rows)
    virtual void render_card(const QzProCardContext &ctx) = 0;
};

// factory: the PRO edition returns its implementation, the LITE edition returns nullptr
std::unique_ptr<QzProHooks> qz_make_pro_hooks(GCodeViewer &viewer);

} // namespace GUI
} // namespace Slic3r
