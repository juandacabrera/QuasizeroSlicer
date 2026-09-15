// Quasizero Slicer — QZmini calibration dialog
// Part of Quasizero Slicer, a fork of OrcaSlicer. GNU AGPLv3.
#pragma once

#include <wx/dialog.h>

class wxTextCtrl;
class wxStaticText;
class wxButton;

namespace Slic3r { namespace GUI {

// Wizard-style dialog implementing the QZmini mechanical calibration:
//  1. E100 test: command E100 (documented), enter measured plunger travel.
//  2. Printed line test: enter path length / width / height / commanded E.
//  3. Derived values: mm/E, mm3/E, ml/E, equivalent virtual filament diameter.
//  4. Apply writes qzmini_* values into the printer preset (saved as user preset
//     through the standard preset save flow). Flow ratio is tuned separately in
//     the filament preset and is never folded into the mechanical calibration.
class QzCalibrationDialog : public wxDialog
{
public:
    explicit QzCalibrationDialog(wxWindow *parent);

private:
    wxTextCtrl *m_barrel_diameter  { nullptr };
    wxTextCtrl *m_e100_commanded   { nullptr };
    wxTextCtrl *m_e100_measured    { nullptr };
    wxTextCtrl *m_line_length      { nullptr };
    wxTextCtrl *m_line_width       { nullptr };
    wxTextCtrl *m_line_height      { nullptr };
    wxTextCtrl *m_line_e           { nullptr };
    wxStaticText *m_result_mm_per_e { nullptr };
    wxStaticText *m_result_mm3_per_e { nullptr };
    wxStaticText *m_result_ml_per_e { nullptr };
    wxStaticText *m_result_virtual_diameter { nullptr };
    wxStaticText *m_warnings       { nullptr };
    double m_mm_per_e { 0.0 };

    void update_results();
    void apply_to_printer_preset();
    double field(wxTextCtrl *ctrl, double fallback) const;
};

}} // namespace Slic3r::GUI
