// Quasizero Slicer — QZmini calibration dialog (implementation)
// Part of Quasizero Slicer, a fork of OrcaSlicer. GNU AGPLv3.
#include "QzCalibrationDialog.hpp"

#include "libslic3r/QuasiZero/QzVolumetricModel.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/Tab.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "libslic3r/PresetBundle.hpp"

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/statbox.h>

namespace Slic3r { namespace GUI {

using QuasiZero::QzVolumetricParams;
using QuasiZero::QzLineTest;

QzCalibrationDialog::QzCalibrationDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, _L("QZmini Calibration"), wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    auto *top = new wxBoxSizer(wxVERTICAL);

    auto add_field = [this](wxSizer *sizer, const wxString &label, const wxString &value) {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(new wxStaticText(this, wxID_ANY, label), 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
        auto *ctrl = new wxTextCtrl(this, wxID_ANY, value, wxDefaultPosition, wxSize(110, -1));
        row->Add(ctrl, 0);
        sizer->Add(row, 0, wxEXPAND | wxALL, 4);
        return ctrl;
    };

    const QzVolumetricParams defaults;

    auto *geo = new wxStaticBoxSizer(wxVERTICAL, this, _L("Syringe geometry"));
    m_barrel_diameter = add_field(geo, _L("Barrel inner diameter (mm)"), wxString::Format("%.2f", defaults.barrel_inner_diameter_mm));
    top->Add(geo, 0, wxEXPAND | wxALL, 8);

    auto *e100 = new wxStaticBoxSizer(wxVERTICAL, this, _L("Step 1 — E100 mechanical test (primary calibration)"));
    e100->Add(new wxStaticText(this, wxID_ANY,
        _L("With the QZmini plunger installed and marked, command a controlled E move\n"
           "(for example: M83, then G1 E100 F300) and measure the physical plunger travel.")),
        0, wxALL, 4);
    m_e100_commanded = add_field(e100, _L("Commanded E (units)"), "100");
    m_e100_measured  = add_field(e100, _L("Measured plunger travel (mm)"), "");
    top->Add(e100, 0, wxEXPAND | wxALL, 8);

    auto *line = new wxStaticBoxSizer(wxVERTICAL, this, _L("Step 2 — Printed line test (provisional seed only)"));
    line->Add(new wxStaticText(this, wxID_ANY,
        _L("Rectangular approximation of a printed line. Use only when the E100\n"
           "measurement is not yet available; it is not a final mechanical calibration.")),
        0, wxALL, 4);
    m_line_length = add_field(line, _L("Path length (mm)"), "20");
    m_line_width  = add_field(line, _L("Line width (mm)"), "4");
    m_line_height = add_field(line, _L("Layer height (mm)"), "3");
    m_line_e      = add_field(line, _L("Commanded E (units)"), "0.9");
    top->Add(line, 0, wxEXPAND | wxALL, 8);

    auto *res = new wxStaticBoxSizer(wxVERTICAL, this, _L("Derived values"));
    m_result_mm_per_e         = new wxStaticText(this, wxID_ANY, "");
    m_result_mm3_per_e        = new wxStaticText(this, wxID_ANY, "");
    m_result_ml_per_e         = new wxStaticText(this, wxID_ANY, "");
    m_result_virtual_diameter = new wxStaticText(this, wxID_ANY, "");
    m_warnings                = new wxStaticText(this, wxID_ANY, "");
    for (wxStaticText *t : { m_result_mm_per_e, m_result_mm3_per_e, m_result_ml_per_e, m_result_virtual_diameter, m_warnings })
        res->Add(t, 0, wxALL, 3);
    top->Add(res, 0, wxEXPAND | wxALL, 8);

    top->Add(new wxStaticText(this, wxID_ANY,
        _L("Flow ratio is a separate final multiplier: fine-tune it in the biomaterial\n"
           "(filament) preset after the mechanical calibration, never instead of it.")),
        0, wxALL, 8);

    auto *btns = new wxBoxSizer(wxHORIZONTAL);
    auto *recalc = new wxButton(this, wxID_ANY, _L("Recalculate"));
    auto *apply  = new wxButton(this, wxID_ANY, _L("Apply to printer settings"));
    auto *close  = new wxButton(this, wxID_CANCEL, _L("Close"));
    btns->Add(recalc, 0, wxRIGHT, 8);
    btns->Add(apply, 0, wxRIGHT, 8);
    btns->AddStretchSpacer();
    btns->Add(close, 0);
    top->Add(btns, 0, wxEXPAND | wxALL, 8);

    recalc->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { update_results(); });
    apply->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { apply_to_printer_preset(); });

    SetSizerAndFit(top);
    update_results();
}

double QzCalibrationDialog::field(wxTextCtrl *ctrl, double fallback) const
{
    double v = 0.0;
    if (ctrl != nullptr && ctrl->GetValue().ToDouble(&v) && v > 0.0)
        return v;
    return fallback;
}

void QzCalibrationDialog::update_results()
{
    QzVolumetricParams p;
    p.barrel_inner_diameter_mm = field(m_barrel_diameter, p.barrel_inner_diameter_mm);

    const double commanded = field(m_e100_commanded, 100.0);
    const double measured  = field(m_e100_measured, 0.0);
    bool from_e100 = measured > 0.0;
    if (from_e100) {
        m_mm_per_e = QuasiZero::plunger_mm_per_e_from_e100(measured, commanded);
    } else {
        QzLineTest lt;
        lt.path_length_mm  = field(m_line_length, lt.path_length_mm);
        lt.line_width_mm   = field(m_line_width, lt.line_width_mm);
        lt.layer_height_mm = field(m_line_height, lt.layer_height_mm);
        lt.commanded_e     = field(m_line_e, lt.commanded_e);
        m_mm_per_e = lt.implied_plunger_mm_per_e(p.barrel_area_mm2());
    }
    p.plunger_mm_per_e_unit = m_mm_per_e;

    m_result_mm_per_e->SetLabel(wxString::Format(_L("Plunger travel per E unit: %.4f mm/E (%s)"),
        m_mm_per_e, from_e100 ? _L("from E100 test") : _L("provisional, from line test")));
    m_result_mm3_per_e->SetLabel(wxString::Format(_L("Effective volume per E unit: %.2f mm3/E"), p.effective_volume_mm3_per_e()));
    m_result_ml_per_e->SetLabel(wxString::Format(_L("Effective volume per E unit: %.4f ml/E"), p.effective_volume_mm3_per_e() / 1000.0));
    m_result_virtual_diameter->SetLabel(wxString::Format(_L("Equivalent virtual filament diameter: %.2f mm"), p.equivalent_virtual_filament_diameter_mm()));

    wxString warn;
    for (const std::string &w : p.consistency_warnings())
        warn += wxString::FromUTF8(w) + "\n";
    m_warnings->SetLabel(warn);
    Layout();
    Fit();
}

void QzCalibrationDialog::apply_to_printer_preset()
{
    update_results();
    if (m_mm_per_e <= 0.0)
        return;
    Tab *tab = wxGetApp().get_tab(Preset::TYPE_PRINTER);
    if (tab == nullptr)
        return;
    // Write through the standard tab mechanism so the change is tracked as a
    // preset modification and can be saved as a user preset by the user.
    tab->load_key_value("qzmini_barrel_inner_diameter", field(m_barrel_diameter, 35.0));
    tab->load_key_value("qzmini_plunger_mm_per_e_unit", m_mm_per_e);
    tab->update_dirty();
}

}} // namespace Slic3r::GUI
