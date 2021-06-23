#include "Notebook.hpp"
#include "GUI_App.hpp"

#include <wx/button.h>
#include <wx/sizer.h>

namespace Slic3r {
namespace GUI {

#ifdef _WIN32

wxDEFINE_EVENT(wxCUSTOMEVT_NOTEBOOK_SEL_CHANGED, wxCommandEvent);

ButtonsListCtrl::ButtonsListCtrl(wxWindow *parent) :
    wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxTAB_TRAVERSAL)
{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif //__WINDOWS__

    m_sizer = new wxBoxSizer(wxHORIZONTAL);
    this->SetSizer(m_sizer);

    this->Bind(wxEVT_PAINT, &ButtonsListCtrl::OnPaint, this);
}

void ButtonsListCtrl::OnPaint(wxPaintEvent&)
{
    GUI::wxGetApp().UpdateDarkUI(this);
    const wxSize sz = GetSize();
    wxPaintDC dc(this);

    if (m_selection < 0 || m_selection >= (int)m_pageButtons.size())
        return;
    // highlight selected button
    for (int idx = 0; idx < m_pageButtons.size(); idx++) {
        wxButton* btn = m_pageButtons[idx];

        btn->SetBackgroundColour(idx == m_selection ? wxGetApp().get_color_selected_btn_bg() : wxGetApp().get_highlight_default_clr());

        wxPoint pos = btn->GetPosition();
        wxSize size = btn->GetSize();
        const wxColour& clr = idx == m_selection ? wxGetApp().get_color_hovered_btn_label() : wxGetApp().get_highlight_default_clr();
        dc.SetPen(clr);
        dc.SetBrush(clr);
        dc.DrawRectangle(pos.x, sz.y - 3, size.x, 3);
    }

    const wxColour& clr_sel = wxGetApp().get_color_hovered_btn_label();
    dc.SetPen(clr_sel);
    dc.SetBrush(clr_sel);
    dc.DrawRectangle(1, sz.y - 1, sz.x, 1);
}

void ButtonsListCtrl::SetSelection(int sel)
{
    if (m_selection == sel)
        return;
    m_selection = sel;
    Refresh();
}

bool ButtonsListCtrl::InsertPage(size_t n, const wxString& text, bool bSelect/* = false*/)
{
    wxButton* btn = new wxButton(this, wxID_ANY, text);
    btn->Bind(wxEVT_BUTTON, [this, btn](wxCommandEvent& event) {
        if (auto it = std::find(m_pageButtons.begin(), m_pageButtons.end(), btn); it != m_pageButtons.end()) {
            m_selection = it - m_pageButtons.begin();
            wxCommandEvent evt = wxCommandEvent(wxCUSTOMEVT_NOTEBOOK_SEL_CHANGED);
            evt.SetId(m_selection);
            wxPostEvent(this->GetParent(), evt);
            Refresh();
        }
    });
    wxGetApp().UpdateDarkUI(btn);
    m_pageButtons.insert(m_pageButtons.begin() + n, btn);
    m_sizer->Insert(n, new wxSizerItem(btn, 0, wxEXPAND | wxRIGHT | wxBOTTOM, 3));
    m_sizer->Layout();
    return true;
}

void ButtonsListCtrl::RemovePage(size_t n)
{
    wxButton* btn = m_pageButtons[n];
    m_pageButtons.erase(m_pageButtons.begin() + n);
    m_sizer->Remove(n);
    btn->Reparent(nullptr);
    btn->Destroy();
    m_sizer->Layout();
}


void ButtonsListCtrl::SetPageText(size_t n, const wxString& strText)
{
    wxButton* btn = m_pageButtons[n];
    btn->SetLabel(strText);
}

wxString ButtonsListCtrl::GetPageText(size_t n) const
{
    wxButton* btn = m_pageButtons[n];
    return btn->GetLabel();
}

#endif // _WIN32

} // GUI
} // Slic3r


