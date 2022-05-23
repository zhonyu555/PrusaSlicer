#ifndef slic3r_Downloader_hpp_
#define slic3r_Downloader_hpp_
#include <wx/wx.h>

namespace Downloader {
class DownloadApp : public wxApp
{
public:
    virtual bool OnInit();
};

class DownloadFrame : public wxFrame
{
public:
    DownloadFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
private:
    void OnHello(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);

    void on_progress(wxCommandEvent& event);
    void on_error(wxCommandEvent& event);
    void on_complete(wxCommandEvent& event);
    wxDECLARE_EVENT_TABLE();

    void log(const wxString& msg);
   
    int m_next_id { 0 };
    int get_next_id() {return ++m_next_id; }

    wxStaticText* m_log_label;
    size_t m_log_lines { 0 };
    wxString m_full_log;
};
}

#endif