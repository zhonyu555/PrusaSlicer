#ifndef slic3r_DownloaderApp_hpp_
#define slic3r_DownloadeAppr_hpp_

#include "InstanceSend.hpp"
#include "Download.hpp"
#include <vector>
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
    
   
    int m_next_id { 0 };
    int get_next_id() {return ++m_next_id; }

    wxStaticText* m_log_label;
    size_t m_log_lines { 0 };
    wxString m_full_log;

    wxDataViewListCtrl* dataview;

    wxString m_url;
    wxString m_path_to_slicer;

    std::unique_ptr<SlicerSend> m_instance_send;

    boost::filesystem::path m_dest_folder;

    std::vector<std::unique_ptr<Download>> m_downloads;
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