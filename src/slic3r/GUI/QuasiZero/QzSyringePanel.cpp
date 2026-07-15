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

// qz_syringe.svg viewBox 0 0 158 586.7; the (removed) #fill path spans
// x in [31,127.4], y in [239.1,491.6]  (y0 = full top, y1 = empty bottom).
static const double VB_W = 158.0, VB_H = 586.7;
static const double FILL_X0 = 31.0, FILL_X1 = 127.4;
static const double FILL_Y0 = 239.1, FILL_Y1 = 491.6;

static const wxColour QZ_BROWN(138, 98, 68);
static const wxColour QZ_TEXT(60, 50, 42);

QzSyringePanel::QzSyringePanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxFULL_REPAINT_ON_RESIZE)
{
    SetBackgroundColour(*wxWHITE);
    m_outline = ScalableBitmap(this, "qz_syringe", 190);
    m_timer.SetOwner(this);
    Bind(wxEVT_TIMER, &QzSyringePanel::on_tick, this);

    auto *root = new wxBoxSizer(wxHORIZONTAL);

    m_draw_area = new wxWindow(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(64), FromDIP(180)),
                               wxFULL_REPAINT_ON_RESIZE);
    m_draw_area->SetBackgroundStyle(wxBG_STYLE_PAINT);
    m_draw_area->SetBackgroundColour(*wxWHITE);
    m_draw_area->SetMinSize(wxSize(FromDIP(56), FromDIP(160)));
    m_draw_area->SetMaxSize(wxSize(FromDIP(80), FromDIP(220)));
    m_draw_area->Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent &) {}); // avoid flicker; paint clears
    m_draw_area->Bind(wxEVT_SIZE, [this](wxSizeEvent &e) { m_draw_area->Refresh(); e.Skip(); });
    m_draw_area->Bind(wxEVT_PAINT, [this](wxPaintEvent &) {
        wxAutoBufferedPaintDC dc(m_draw_area);
        dc.SetBackground(*wxWHITE_BRUSH);
        dc.Clear();
        paint_syringe(dc, m_draw_area->GetClientSize());
    });
    root->Add(m_draw_area, 0, wxALL, FromDIP(4));

    auto *col = new wxBoxSizer(wxVERTICAL);
    auto mk_text = [this](const wxString &t, const wxFont &f, const wxColour &c) {
        auto *st = new wxStaticText(this, wxID_ANY, t);
        st->SetFont(f);
        st->SetForegroundColour(c);
        st->SetBackgroundColour(*wxWHITE);
        return st;
    };
    col->Add(mk_text(_L("QZmini syringe"), ::Label::Head_14, QZ_BROWN), 0, wxBOTTOM, FromDIP(4));
    m_pct_label = mk_text("100 %", ::Label::Head_20, QZ_TEXT);
    col->Add(m_pct_label, 0, wxBOTTOM, FromDIP(2));
    m_ml_label = mk_text("150.0 ml", ::Label::Body_12, wxColour(120, 108, 96));
    col->Add(m_ml_label, 0, wxBOTTOM, FromDIP(10));
    col->Add(mk_text(_L("Manual plunger (cold)"), ::Label::Body_12, wxColour(120, 108, 96)), 0, wxBOTTOM, FromDIP(4));

    // Native Device-panel button style (like the extruder / Load-Unload / Lamp
    // buttons): light-gray fill, a coloured border appears on hover, the fill
    // darkens on press, and a disabled button is greyed out.
    static const wxColour BTN_NORMAL(238, 238, 238);
    static const wxColour BTN_PRESS(172, 172, 172);
    static const wxColour BTN_HOVER(138, 98, 68); // Quasizero brown border on hover
    auto make_jog = [&](const wxString &icon) {
        auto *b = new ::Button(this, "", icon, 0, 22);
        b->SetBorderWidth(2);
        b->SetMinSize(wxSize(FromDIP(40), FromDIP(38)));
        b->SetCornerRadius(4);
        b->SetBackgroundColor(StateColor(std::pair{BTN_PRESS, (int)StateColor::Pressed},
                                         std::pair{BTN_NORMAL, (int)StateColor::Normal}));
        b->SetBorderColor(StateColor(std::pair{BTN_HOVER, (int)StateColor::Hovered},
                                     std::pair{BTN_NORMAL, (int)StateColor::Normal}));
        return b;
    };
    m_btn_up   = make_jog("monitor_extruder_up");   // retract plunger
    m_btn_down = make_jog("monitor_extruder_down"); // extrude
    m_btn_play = make_jog("media_stop");            // stop
    m_btn_play->Enable(false); // idle: greyed out, like a disabled Unload

    auto *row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(m_btn_up, 0, wxRIGHT, FromDIP(6));
    row->Add(m_btn_down, 0, wxRIGHT, FromDIP(6));
    row->Add(m_btn_play, 0);
    col->Add(row, 0);

    m_btn_up  ->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { start_jog(-1); });
    m_btn_down->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { start_jog(+1); });
    m_btn_play->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { stop_jog(); });

    root->Add(col, 0, wxALL, FromDIP(8));
    root->AddStretchSpacer(1);
    SetSizer(root);
    SetMinSize(wxSize(FromDIP(260), FromDIP(200)));
}

void QzSyringePanel::paint_syringe(wxDC &dc, const wxSize &sz)
{
    if (sz.x <= 0 || sz.y <= 0) return;
    const double scale = std::min(sz.x / VB_W, sz.y / VB_H);
    const double ox = (sz.x - VB_W * scale) / 2.0;
    const double oy = (sz.y - VB_H * scale) / 2.0;

    const double frac = std::min(1.0, std::max(0.0, m_fraction));
    const double level_y = FILL_Y1 - frac * (FILL_Y1 - FILL_Y0);
    const int x    = (int)std::lround(ox + FILL_X0 * scale);
    const int w    = (int)std::lround((FILL_X1 - FILL_X0) * scale);
    const int ytop = (int)std::lround(oy + level_y * scale);
    const int h    = (int)std::lround(oy + FILL_Y1 * scale) - ytop;
    if (h > 0 && w > 0) {
        dc.SetBrush(wxBrush(m_material_colour));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRoundedRectangle(x, ytop, w, h, std::min(w, h) * 0.16);
    }
    const wxBitmap &bmp = m_outline.bmp();
    if (bmp.IsOk()) {
        const double bs = std::min(sz.x / (double)bmp.GetWidth(), sz.y / (double)bmp.GetHeight());
        const int bw = std::max(1, (int)std::lround(bmp.GetWidth() * bs));
        const int bh = std::max(1, (int)std::lround(bmp.GetHeight() * bs));
        wxImage img = bmp.ConvertToImage();
        if (img.IsOk()) {
            img = img.Scale(bw, bh, wxIMAGE_QUALITY_HIGH);
            dc.DrawBitmap(wxBitmap(img), (int)std::lround((sz.x - bw) / 2.0), (int)std::lround((sz.y - bh) / 2.0), true);
        }
    }
}

static void qz_set_active(::Button *b, bool active)
{
    if (!b) return;
    static const wxColour N(238, 238, 238), P(172, 172, 172), H(138, 98, 68);
    if (active) {
        // held look: brown border stays on, fill slightly darkened
        b->SetBorderColor(StateColor(std::pair{H, (int)StateColor::Normal}));
        b->SetBackgroundColor(StateColor(std::pair{wxColour(0xEA, 0xDF, 0xD4), (int)StateColor::Normal}));
    } else {
        b->SetBorderColor(StateColor(std::pair{H, (int)StateColor::Hovered}, std::pair{N, (int)StateColor::Normal}));
        b->SetBackgroundColor(StateColor(std::pair{P, (int)StateColor::Pressed}, std::pair{N, (int)StateColor::Normal}));
    }
    b->Refresh();
}

void QzSyringePanel::start_jog(int dir)
{
    m_dir = dir;
    qz_set_active(m_btn_up,   dir < 0);
    qz_set_active(m_btn_down, dir > 0);
    if (m_btn_play) { m_btn_play->Enable(true); m_btn_play->Refresh(); }
    if (on_manual_extrude) on_manual_extrude(dir * m_step_e, m_feedrate); // immediate step
    if (!m_timer.IsRunning()) m_timer.Start(m_tick_ms);
}

void QzSyringePanel::stop_jog()
{
    m_dir = 0;
    if (m_timer.IsRunning()) m_timer.Stop();
    qz_set_active(m_btn_up, false);
    qz_set_active(m_btn_down, false);
    if (m_btn_play) { m_btn_play->Enable(false); m_btn_play->Refresh(); }
}

void QzSyringePanel::on_tick(wxTimerEvent &)
{
    if (m_dir != 0 && on_manual_extrude) on_manual_extrude(m_dir * m_step_e, m_feedrate);
}

void QzSyringePanel::set_remaining_fraction(double f)
{
    m_fraction = std::min(1.0, std::max(0.0, f));
    if (m_pct_label) m_pct_label->SetLabel(wxString::Format("%d %%", (int)std::lround(m_fraction * 100)));
    if (m_ml_label)  m_ml_label->SetLabel(wxString::Format("%.1f ml", m_fraction * m_nominal_ml));
    if (m_draw_area) m_draw_area->Refresh();
}

void QzSyringePanel::set_material_colour(const wxColour &c)
{
    if (c.IsOk()) { m_material_colour = c; if (m_draw_area) m_draw_area->Refresh(); }
}

}} // namespace Slic3r::GUI
