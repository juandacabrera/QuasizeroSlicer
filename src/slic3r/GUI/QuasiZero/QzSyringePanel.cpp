// Quasizero Slicer — QZmini syringe status widget (implementation). GNU AGPLv3.
#include "QzSyringePanel.hpp"

#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/Widgets/Label.hpp"

#include <wx/dcbuffer.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <algorithm>
#include <cmath>

namespace Slic3r { namespace GUI {

// qz_syringe.svg viewBox is 0 0 158 586.7; the (removed) #fill path occupies
// x in [31, 127.4] and y in [239.1, 491.6]. These proportions place the
// material fill exactly inside the drawn outline at any widget size.
static const double VB_W = 158.0, VB_H = 586.7;
static const double FILL_X0 = 31.0, FILL_X1 = 127.4;
static const double FILL_Y0 = 239.1, FILL_Y1 = 491.6; // y0 = full top, y1 = empty bottom

QzSyringePanel::QzSyringePanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(*wxWHITE);
    m_outline = ScalableBitmap(this, "qz_syringe", 160);

    auto *root = new wxBoxSizer(wxHORIZONTAL);

    // Left: the drawn syringe (this panel paints it), reserve space for it.
    auto *draw_area = new wxWindow(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(70), FromDIP(190)));
    draw_area->SetBackgroundColour(*wxWHITE);
    draw_area->Bind(wxEVT_PAINT, [this, draw_area](wxPaintEvent &) {
        wxAutoBufferedPaintDC dc(draw_area);
        dc.SetBackground(*wxWHITE_BRUSH);
        dc.Clear();
        wxSize sz = draw_area->GetClientSize();
        if (sz.x <= 0 || sz.y <= 0) return;
        const double scale = std::min(sz.x / VB_W, sz.y / VB_H);
        const double ox = (sz.x - VB_W * scale) / 2.0;
        const double oy = (sz.y - VB_H * scale) / 2.0;

        // Material fill: drains top-down. Filled band from level to bottom.
        const double frac = (m_fraction < 0.0) ? 0.0 : (m_fraction > 1.0 ? 1.0 : m_fraction);
        const double level_y = FILL_Y1 - frac * (FILL_Y1 - FILL_Y0);
        const int x    = (int)std::lround(ox + FILL_X0 * scale);
        const int w    = (int)std::lround((FILL_X1 - FILL_X0) * scale);
        const int ytop = (int)std::lround(oy + level_y * scale);
        const int h    = (int)std::lround(oy + FILL_Y1 * scale) - ytop;
        if (h > 0 && w > 0) {
            dc.SetBrush(wxBrush(m_material_colour));
            dc.SetPen(*wxTRANSPARENT_PEN);
            const double r = std::min(w, h) * 0.18;
            dc.DrawRoundedRectangle(x, ytop, w, h, r);
        }
        // Outline on top (transparent interior lets the fill show through).
        const wxBitmap &bmp = m_outline.bmp();
        if (bmp.IsOk()) {
            const double bs = std::min(sz.x / (double)bmp.GetWidth(), sz.y / (double)bmp.GetHeight());
            const int bw = std::max(1, (int)std::lround(bmp.GetWidth() * bs));
            const int bh = std::max(1, (int)std::lround(bmp.GetHeight() * bs));
            wxImage img = bmp.ConvertToImage();
            if (img.IsOk()) {
                img = img.Scale(bw, bh, wxIMAGE_QUALITY_HIGH);
                wxBitmap scaled(img);
                const int bx = (int)std::lround((sz.x - bw) / 2.0);
                const int by = (int)std::lround((sz.y - bh) / 2.0);
                dc.DrawBitmap(scaled, bx, by, true);
            }
        }
    });
    root->Add(draw_area, 0, wxALL, FromDIP(4));

    // Right: title, percentage/ml, manual extrude buttons.
    auto *col = new wxBoxSizer(wxVERTICAL);
    auto *title = new Label(this, _L("QZmini syringe"));
    title->SetFont(::Label::Head_14);
    title->SetForegroundColour(wxColour(138, 98, 68));
    col->Add(title, 0, wxBOTTOM, FromDIP(6));

    m_pct_label = new wxStaticText(this, wxID_ANY, "100 %");
    m_pct_label->SetFont(::Label::Head_20);
    m_pct_label->SetForegroundColour(wxColour(60, 50, 42));
    col->Add(m_pct_label, 0, wxBOTTOM, FromDIP(2));

    m_ml_label = new wxStaticText(this, wxID_ANY, "150.0 ml");
    m_ml_label->SetForegroundColour(wxColour(120, 108, 96));
    col->Add(m_ml_label, 0, wxBOTTOM, FromDIP(10));

    auto *hint = new wxStaticText(this, wxID_ANY, _L("Manual plunger (cold)"));
    hint->SetForegroundColour(wxColour(120, 108, 96));
    col->Add(hint, 0, wxBOTTOM, FromDIP(4));

    auto make_btn = [this](const wxString &label) {
        auto *b = new Button(this, label);
        b->SetMinSize(wxSize(FromDIP(48), FromDIP(28)));
        b->SetCornerRadius(4);
        StateColor bg(std::pair{wxColour(154, 109, 74), (int)StateColor::Hovered},
                      std::pair{wxColour(138, 98, 68), (int)StateColor::Normal});
        b->SetBackgroundColor(bg);
        b->SetTextColor(StateColor(std::pair{wxColour(255, 255, 255), (int)StateColor::Normal}));
        return b;
    };
    auto *row = new wxBoxSizer(wxHORIZONTAL);
    auto *minus = make_btn(wxString::FromUTF8("−"));   // minus sign, plunger back
    auto *plus  = make_btn("+");          // plunger forward
    row->Add(minus, 0, wxRIGHT, FromDIP(6));
    row->Add(plus, 0);
    col->Add(row, 0);

    double step_e = m_step_ml; // 1 ml step; the caller converts ml->E if desired
    minus->Bind(wxEVT_BUTTON, [this, step_e](wxCommandEvent &) { if (on_manual_extrude) on_manual_extrude(-step_e, m_feedrate); });
    plus ->Bind(wxEVT_BUTTON, [this, step_e](wxCommandEvent &) { if (on_manual_extrude) on_manual_extrude(+step_e, m_feedrate); });

    root->Add(col, 1, wxALL, FromDIP(8));
    SetSizer(root);
    m_draw_area = draw_area;
}

void QzSyringePanel::set_remaining_fraction(double f)
{
    m_fraction = std::clamp(f, 0.0, 1.0);
    if (m_pct_label) m_pct_label->SetLabel(wxString::Format("%d %%", (int)std::lround(m_fraction * 100)));
    if (m_ml_label)  m_ml_label->SetLabel(wxString::Format("%.1f ml", m_fraction * m_nominal_ml));
    if (m_draw_area) m_draw_area->Refresh();
}

void QzSyringePanel::set_material_colour(const wxColour &c)
{
    if (c.IsOk()) { m_material_colour = c; if (m_draw_area) m_draw_area->Refresh(); }
}

void QzSyringePanel::on_paint(wxPaintEvent &) {}
void QzSyringePanel::render(wxDC &) {}

}} // namespace Slic3r::GUI
