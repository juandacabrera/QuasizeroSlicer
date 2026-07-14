// Quasizero Slicer — QZmini syringe status widget for the Device page.
// Part of Quasizero Slicer, a fork of OrcaSlicer. GNU AGPLv3.
#pragma once

#include <wx/panel.h>
#include <wx/colour.h>
#include <wx/timer.h>
#include <functional>
#include "slic3r/GUI/wxExtensions.hpp"

class wxStaticText;
class Button; // global Widgets/Button.hpp

namespace Slic3r { namespace GUI {

class QzSyringePanel : public wxPanel
{
public:
    QzSyringePanel(wxWindow *parent);

    void   set_remaining_fraction(double f);
    double remaining_fraction() const { return m_fraction; }
    void   set_material_colour(const wxColour &c);
    void   set_nominal_capacity_ml(double ml) { m_nominal_ml = ml; }

    // Called for each firm jog step: (delta_e_units, feedrate_mm_min).
    // Positive pushes the plunger DOWN (extrude); negative pulls it UP (retract).
    std::function<void(double /*delta_e*/, int /*feedrate*/)> on_manual_extrude;

private:
    ScalableBitmap m_outline;
    wxColour m_material_colour{0xC9, 0xA4, 0x7E};
    double   m_fraction   = 1.0;
    double   m_nominal_ml = 150.0;

    // Continuous jog (start with an arrow, stop with the play/stop button).
    wxTimer  m_timer;
    int      m_dir = 0;            // -1 up/retract, +1 down/extrude, 0 idle
    double   m_step_e     = 5.0;   // firm step per tick (E units)
    int      m_feedrate   = 600;   // mm/min, firm (near native jog feel)
    int      m_tick_ms    = 350;

    wxWindow    *m_draw_area  = nullptr;
    wxStaticText *m_pct_label = nullptr;
    wxStaticText *m_ml_label  = nullptr;
    ::Button    *m_btn_play   = nullptr;

    void start_jog(int dir);
    void stop_jog();
    void on_tick(wxTimerEvent &);
    void paint_syringe(wxDC &dc, const wxSize &sz);
};

}} // namespace Slic3r::GUI
