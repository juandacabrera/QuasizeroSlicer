// Quasizero Slicer — QZmini syringe status widget for the Device page.
// Part of Quasizero Slicer, a fork of OrcaSlicer. GNU AGPLv3.
//
// Draws the QZmini syringe (outline SVG) with a material-coloured fill that
// drains top-down as biomaterial is consumed, plus a remaining-percentage
// label and two manual COLD-extrude buttons (plunger forward / backward).
#pragma once

#include <wx/panel.h>
#include <wx/colour.h>
#include <functional>

class wxStaticText;
#include "slic3r/GUI/wxExtensions.hpp"

namespace Slic3r { namespace GUI {

class QzSyringePanel : public wxPanel
{
public:
    QzSyringePanel(wxWindow *parent);

    // Remaining biomaterial as a fraction of the nominal syringe capacity [0..1].
    void   set_remaining_fraction(double f);
    double remaining_fraction() const { return m_fraction; }

    // Colour of the biomaterial (from the filament preset's default colour).
    void set_material_colour(const wxColour &c);

    // Nominal syringe capacity, ml (for the "X ml" readout).
    void set_nominal_capacity_ml(double ml) { m_nominal_ml = ml; Refresh(); }

    // Called when the user presses +/- : (delta_e_units, feedrate_mm_min).
    // Positive pushes the plunger forward (extrude), negative pulls it back.
    std::function<void(double /*delta_e*/, int /*feedrate*/)> on_manual_extrude;

private:
    ScalableBitmap m_outline;      // syringe outline (transparent interior)
    wxColour m_material_colour{0xDC, 0xCB, 0xBE};
    double   m_fraction   = 1.0;
    double   m_nominal_ml = 150.0;
    double   m_step_ml    = 1.0;   // manual extrude step
    int      m_feedrate   = 300;   // mm/min, slow cold extrude

    wxWindow    *m_draw_area  = nullptr;
    wxStaticText *m_pct_label = nullptr;
    wxStaticText *m_ml_label  = nullptr;

    void on_paint(wxPaintEvent &);
    void render(wxDC &dc);
};

}} // namespace Slic3r::GUI
