#include "Notebook.hpp"

//#ifdef _WIN32

#include "GUI_App.hpp"
#include "wxExtensions.hpp"
#include "Widgets/Button.hpp"

//BBS set font size
#include "Widgets/Label.hpp"

#include <wx/button.h>
#include <wx/sizer.h>

wxDEFINE_EVENT(wxCUSTOMEVT_NOTEBOOK_SEL_CHANGED, wxCommandEvent);

ButtonsListCtrl::ButtonsListCtrl(wxWindow *parent, wxBoxSizer* side_tools) :
    wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxTAB_TRAVERSAL)
{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif //__WINDOWS__

    wxColour default_btn_bg;
#ifdef __APPLE__
    default_btn_bg = wxColour("#F5F5F4"); // Quasizero arena
#else
    default_btn_bg = wxColour("#F5F5F4"); // Quasizero arena
#endif

   
    SetBackgroundColour(default_btn_bg);

    int em = em_unit(this);// Slic3r::GUI::wxGetApp().em_unit();
    // BBS: no gap
    m_btn_margin = 0; // std::lround(0.3 * em);
    m_line_margin = std::lround(0.1 * em);

    m_sizer = new wxBoxSizer(wxHORIZONTAL);
    this->SetSizer(m_sizer);

    m_buttons_sizer = new wxFlexGridSizer(1, m_btn_margin, m_btn_margin);
    m_sizer->AddStretchSpacer(1); // Quasizero: center the page pills
    m_sizer->Add(m_buttons_sizer, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxBOTTOM, m_btn_margin);

    if (side_tools != NULL) {
        m_sizer->AddStretchSpacer(1);
        for (size_t idx = 0; idx < side_tools->GetItemCount(); idx++) {
            wxSizerItem* item = side_tools->GetItem(idx);
            wxWindow* item_win = item->GetWindow();
            if (item_win) {
                item_win->Reparent(this);
            }
        }
        m_sizer->Add(side_tools, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxBOTTOM, m_btn_margin);
    }

    if (side_tools == NULL)
        m_sizer->AddStretchSpacer(1); // keep the pills centered when there are no right-side tools

    this->Bind(wxEVT_PAINT, &ButtonsListCtrl::OnPaint, this); // Quasizero: continuous white bar with rounded ends
    Bind(wxEVT_SYS_COLOUR_CHANGED, [this](auto& e){
    });
}

void ButtonsListCtrl::OnPaint(wxPaintEvent&)
{
    // Quasizero: strip background + one continuous white bar with fully rounded
    // ends behind the page buttons (the buttons themselves are square white, so
    // the whole group reads as a single pill).
    wxPaintDC dc(this);
    const wxSize sz = GetSize();
    const wxColour strip(245, 245, 244);
    dc.SetPen(wxPen(strip));
    dc.SetBrush(wxBrush(strip));
    dc.DrawRectangle(0, 0, sz.x, sz.y);

    int x0 = 1 << 29, y0 = 1 << 29, x1 = -1, y1 = -1;
    for (Button* btn : m_pageButtons) {
        if (btn == nullptr || !btn->IsShown()) continue;
        const wxPoint p = btn->GetPosition();
        const wxSize  bs = btn->GetSize();
        x0 = std::min(x0, p.x);        y0 = std::min(y0, p.y);
        x1 = std::max(x1, p.x + bs.x); y1 = std::max(y1, p.y + bs.y);
    }
    if (x1 > x0 && y1 > y0) {
        const int h   = y1 - y0;
        const int pad = h / 2;
        dc.SetPen(*wxWHITE_PEN);
        dc.SetBrush(*wxWHITE_BRUSH);
        dc.DrawRoundedRectangle(x0 - pad, y0, (x1 - x0) + 2 * pad, h, h / 2.0);
    }
}

void ButtonsListCtrl::UpdateMode()
{
    //m_mode_sizer->SetMode(Slic3r::GUI::wxGetApp().get_mode());
}

void ButtonsListCtrl::Rescale()
{
    //m_mode_sizer->msw_rescale();
    int em = em_unit(this);
    for (Button* btn : m_pageButtons) {
        //BBS
        btn->SetMinSize({(btn->GetLabel().empty() ? 40 : 132) * em / 10, 36 * em / 10});
        btn->Rescale();
    }

    // BBS: no gap
    //m_btn_margin = std::lround(0.3 * em);
    //m_line_margin = std::lround(0.1 * em);
    //m_buttons_sizer->SetVGap(m_btn_margin);
    //m_buttons_sizer->SetHGap(m_btn_margin);

    m_sizer->Layout();
}

void ButtonsListCtrl::SetSelection(int sel)
{
    if (m_selection == sel)
        return;
    // BBS: change button color
    wxColour selected_btn_bg("#3A3835");    // Gradient #3A3835
    if (m_selection >= 0) {
        StateColor bg_color = StateColor(
        std::pair{wxColour(236, 235, 233), (int) StateColor::Hovered},
        std::pair{wxColour(255, 255, 255), (int) StateColor::Normal}); // Quasizero: white, part of the continuous pill bar
        m_pageButtons[m_selection]->SetBackgroundColor(bg_color);
        StateColor text_color = StateColor(
        std::pair{wxColour(31, 31, 31), (int) StateColor::Normal}
        );
        m_pageButtons[m_selection]->SetSelected(false);
        m_pageButtons[m_selection]->SetTextColor(text_color);
    }
    m_selection = sel;

    StateColor bg_color = StateColor(
        std::pair{wxColour(224, 222, 219), (int) StateColor::Hovered},
        std::pair{wxColour(233, 231, 228), (int) StateColor::Normal});
    m_pageButtons[m_selection]->SetBackgroundColor(bg_color);

    StateColor text_color = StateColor(
        std::pair{wxColour(31, 31, 31), (int) StateColor::Normal}
        );
    m_pageButtons[m_selection]->SetSelected(true);
    m_pageButtons[m_selection]->SetTextColor(text_color);
    
    Refresh();
}

bool ButtonsListCtrl::InsertPage(size_t n, const wxString &text, bool bSelect /* = false*/, const std::string &bmp_name /* = ""*/, const std::string &inactive_bmp_name)
{
    Button * btn = new Button(this, text.empty() ? text : " " + text, bmp_name, wxNO_BORDER);
    btn->SetCornerRadius(0);

    int em = em_unit(this);
    //BBS set size for button
    btn->SetMinSize({(text.empty() ? 40 : 136) * em / 10, 36 * em / 10});

    StateColor bg_color = StateColor(
        std::pair{wxColour(236, 235, 233), (int) StateColor::Hovered},
        std::pair{wxColour(255, 255, 255), (int) StateColor::Normal}); // Quasizero: white, part of the continuous pill bar

    btn->SetBackgroundColor(bg_color);
    StateColor text_color = StateColor(
        std::pair{wxColour(31, 31, 31), (int) StateColor::Normal});
    btn->SetTextColor(text_color);
    btn->SetInactiveIcon(inactive_bmp_name);
    btn->SetSelected(false);
    btn->Bind(wxEVT_BUTTON, [this, btn](wxCommandEvent& event) {
        if (auto it = std::find(m_pageButtons.begin(), m_pageButtons.end(), btn); it != m_pageButtons.end()) {
            auto sel = it - m_pageButtons.begin();
            //do it later
            //SetSelection(sel);
            
            wxCommandEvent evt = wxCommandEvent(wxCUSTOMEVT_NOTEBOOK_SEL_CHANGED);
            evt.SetId(sel);
            wxPostEvent(this->GetParent(), evt);
        }
    });
    Slic3r::GUI::wxGetApp().UpdateDarkUI(btn);
    m_pageButtons.insert(m_pageButtons.begin() + n, btn);
    m_pageLabels.insert(m_pageLabels.begin() + n, text); // ORCA
    m_buttons_sizer->Insert(n, new wxSizerItem(btn));
    m_buttons_sizer->SetCols(m_buttons_sizer->GetCols() + 1);
    m_sizer->Layout();
    return true;
}

void ButtonsListCtrl::RemovePage(size_t n)
{
    Button* btn = m_pageButtons[n];
    m_pageButtons.erase(m_pageButtons.begin() + n);
    m_pageLabels.erase(m_pageLabels.begin() + n); // ORCA
    m_buttons_sizer->Remove(n);
#if __WXOSX__
    RemoveChild(btn);
#else
    btn->Reparent(nullptr);
#endif
    btn->Destroy();
    m_sizer->Layout();
}

bool ButtonsListCtrl::SetPageImage(size_t n, const std::string& bmp_name) const
{
    if (n >= m_pageButtons.size())
        return false;
     
    // BBS
    //return m_pageButtons[n]->SetBitmap_(bmp_name);
    ScalableBitmap bitmap(NULL, bmp_name);
    //m_pageButtons[n]->SetBitmap_(bitmap);
    return true;
}

void ButtonsListCtrl::SetPageText(size_t n, const wxString& strText)
{
    Button* btn = m_pageButtons[n];
    btn->SetLabel(strText);
    if(!strText.empty())  // ORCA
        m_pageLabels[n] = strText;
}

// ORCA
void ButtonsListCtrl::SetCompact(size_t n, bool compact)
{
    int em = em_unit(this);
    Button* btn = m_pageButtons[n];
    btn->SetMinSize({(compact ? 40 : 136) * em / 10, 36 * em / 10});
    btn->SetLabel(compact ? "" : (" " +  m_pageLabels[n]));
}

wxString ButtonsListCtrl::GetPageText(size_t n) const
{
    Button* btn = m_pageButtons[n];
    return btn->GetLabel();
}

//#endif // _WIN32

void Notebook::Init()
{
    // We don't need any border as we don't have anything to separate the
    // page contents from.
    SetInternalBorder(0);

    // No effects by default.
    m_showEffect = m_hideEffect = wxSHOW_EFFECT_NONE;

    m_showTimeout = m_hideTimeout = 0;

    /* On Linux, Gstreamer wxMediaCtrl does not seem to get along well with
     * 32-bit X11 visuals (the overlay does not work).  Is this a wxWindows
     * bug?  Is this a Gstreamer bug?  No idea, but it is our problem ... 
     * and anyway, this transparency thing just isn't all that interesting,
     * so we just don't do it on Linux. 
     */
#ifndef __WXGTK__
    SetBackgroundStyle(wxBG_STYLE_TRANSPARENT);
#endif
}
