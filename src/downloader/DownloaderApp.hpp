#ifndef slic3r_DownloaderApp_hpp_
#define slic3r_DownloadeAppr_hpp_

#include "InstanceSend.hpp"
#include "Download.hpp"
#include <vector>
#include <map>
#include <wx/wx.h>
#include <wx/dataview.h>

namespace Downloader {

class DownloadFrame : public wxFrame
{
public:
    DownloadFrame(const wxString& title, const wxPoint& pos, const wxSize& size);

    void set_path_to_slicer(wxString path) { m_path_to_slicer = path; }
    void start_download(wxString url);

    void log(const wxString& msg);
    void handle_message(const wxString& msg);
private:

    void on_progress(wxCommandEvent& event);
    void on_error(wxCommandEvent& event);
    void on_complete(wxCommandEvent& event);
    void on_name_change(wxCommandEvent& event);
    void on_open_in_slicer(wxCommandEvent& event);
    void on_open_in_new_slicer(wxCommandEvent& event);
    void on_open_in_explorer(wxCommandEvent& event);
    void on_cancel_button(wxCommandEvent& event);
    void on_pause_button(wxCommandEvent& event);
    void on_resume_button(wxCommandEvent& event);
    
    void update_state_labels();
    void start_next();
    void set_download_state(int id, DownloadState state);
    bool is_in_state(int id, DownloadState state) const;
    DownloadState get_download_state(int id) const;
    bool cancel_download(int id);
    bool pause_download(int id);
    bool resume_download(int id);
    bool delete_download(int id);
    wxString get_path_of(int id) const;
    wxString get_folder_path_of(int id) const;

    int m_next_id { 0 };
    int get_next_id() {return ++m_next_id; }

    wxStaticText* m_log_label;
    size_t m_log_lines { 0 };
    wxString m_full_log;

    wxDataViewListCtrl* m_dataview;

    wxString m_url;
    wxString m_path_to_slicer;

    std::unique_ptr<SlicerSend> m_slicer_send;

    boost::filesystem::path m_dest_folder;

    std::vector<std::unique_ptr<Download>> m_downloads;
    /* DownloadPending = 0,
    DownloadOngoing,
    DownloadStopped,
    DownloadDone,
    DownloadError,
    DownloadPaused*/
    const std::map<DownloadState, wxString> c_state_labels = {
        {DownloadPending,   "Pending"},
        {DownloadStopped,   "Canceled"},
        {DownloadDone,      "Done"},
        {DownloadError,     "Error"},
        {DownloadPaused,    "Paused"},
    };
};

class DownloadApp : public wxApp
{
protected:
    DownloadFrame* m_frame{ nullptr };
    std::unique_ptr<DownloaderSend> m_dwnldr_send;

    bool m_other_exists { false };

public:
    bool OnInit() override;
    void OnInitCmdLine(wxCmdLineParser& parser) override;
    bool OnCmdLineParsed(wxCmdLineParser& parser) override;
    
    
};

wxDECLARE_APP(DownloadApp);


//
//
//
//
//
//class TextRenderer : public wxDataViewCustomRenderer
//{
//public:
//    TextRenderer(wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT
//        , int align = wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL
//    ) : wxDataViewCustomRenderer(wxT("string"), mode, align) {}
//
//    bool SetValue(const wxVariant& value) override;
//    bool GetValue(wxVariant& value) const override;
//
//    virtual bool Render(wxRect cell, wxDC* dc, int state) override;
//    virtual wxSize GetSize() const override;
//
//    bool        HasEditorCtrl() const override { return false; }
//
//private:
//    wxString    m_value;
//};
//
//


}

#endif