#include "DownloaderApp.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <boost/nowide/convert.hpp>
#include <wx/event.h>
#include <wx/cmdline.h>

namespace Downloader {

bool DownloadApp::OnInit()
{
    m_dwnldr_send = std::make_unique<DownloaderSend>();
   

    if (m_dwnldr_send->get_instance_exists()) {
        m_other_exists = true;
        m_frame = new DownloadFrame("PrusaSlicer-Downloader", wxPoint(50, 50), wxSize(0,0));
        return wxApp::OnInit();
    } 
    m_frame = new DownloadFrame("PrusaSlicer-Downloader", wxPoint(50, 50), wxSize(450, 340));
    m_frame->Show(true);
   
    wxWindow::MSWRegisterMessageHandler(WM_COPYDATA, [](wxWindow* win, WXUINT /* nMsg */, WXWPARAM wParam, WXLPARAM lParam) {
        auto frame = dynamic_cast<DownloadFrame*>(win);
        COPYDATASTRUCT* copy_data_structure = { 0 };
        copy_data_structure = (COPYDATASTRUCT*)lParam;
        if (copy_data_structure->dwData == 1) {
            LPCWSTR arguments = (LPCWSTR)copy_data_structure->lpData;
            frame->handle_message(arguments);
        }
        return true;
    });

    return wxApp::OnInit();
}

void DownloadApp::OnInitCmdLine(wxCmdLineParser& parser)
{
    static const wxCmdLineEntryDesc cmdLineDesc[] =
    {
        { wxCMD_LINE_SWITCH, "v", "verbose", "be verbose" },
        { wxCMD_LINE_SWITCH, "q", "quiet",   "be quiet" },

//        { wxCMD_LINE_OPTION, "s", "slicer",    "path to prusaslicer", wxCMD_LINE_VAL_STRING },
        { wxCMD_LINE_OPTION, "u", "url",    "url to download", wxCMD_LINE_VAL_STRING },

        { wxCMD_LINE_NONE }
    };
    
    parser.SetDesc(cmdLineDesc);

}

bool DownloadApp::OnCmdLineParsed(wxCmdLineParser& parser)
{
    wxString option;
    wxString url;
    if (parser.Found("u", &option)) {
        url = option;
    }

    if (m_other_exists) {
        m_dwnldr_send->send_url(url);
        m_frame->log("sent " + url);
        m_frame->Close(true);
        return false;
    } else {
        if (!url.empty() && m_frame != nullptr)
            m_frame->start_download(std::move(url));

        return wxApp::OnCmdLineParsed(parser);
    } 
}



DownloadFrame::DownloadFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
    : wxFrame(NULL, wxID_ANY, title, pos, size)
    , m_instance_send(std::make_unique<SlicerSend>())
{

    dataview = new wxDataViewListCtrl(this, wxID_ANY);

   /* dataview->AppendColumn(new wxDataViewColumn("ID", new TextRenderer(), 0, 100, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE));
    dataview->AppendColumn(new wxDataViewColumn("Filename", new TextRenderer(), 1, 100, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE));
    dataview->AppendProgressColumn("Progress", wxDATAVIEW_CELL_INERT, 100, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    dataview->AppendColumn(new wxDataViewColumn("status", new TextRenderer(), 2, 100, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE));*/


    dataview->AppendTextColumn("ID", wxDATAVIEW_CELL_INERT, 30, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    dataview->AppendTextColumn("Filename", wxDATAVIEW_CELL_INERT, 100, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    dataview->AppendProgressColumn("Progress", wxDATAVIEW_CELL_INERT, 100, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    dataview->AppendTextColumn("status", wxDATAVIEW_CELL_INERT, 100, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);



    m_dest_folder = boost::filesystem::path("C:\\Users\\User\\Downloads");

    m_log_label = new wxStaticText(this, wxID_ANY, "Log:");
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    //sizer->Add(m_log_label, wxEXPAND);
    sizer->Add(dataview, 1, wxEXPAND | wxBOTTOM);
    SetSizer(sizer);

    Bind(EVT_FILE_COMPLETE, &DownloadFrame::on_complete, this);
    Bind(EVT_FILE_PROGRESS, &DownloadFrame::on_progress, this);
    Bind(EVT_FILE_ERROR, &DownloadFrame::on_error, this);
}

void DownloadFrame::start_download(wxString url)
{
//    prusaslicer://open?file=https%3A%2F%2Fmedia.printables.com%2Fmedia%2Fprints%2F152208%2Fstls%2F1431590_8b8287b3-03b1-4cbe-82d0-268a0affa171%2Ff1_logo.stl
    if (url.starts_with("open?file=")) {
        int id = get_next_id();
        std::string escaped_url = FileGet::escape_url(boost::nowide::narrow(url.substr(10)));
        log(std::to_string(id) + ": start " + escaped_url);
        m_downloads.emplace_back(std::make_unique<Download>(id, std::move(escaped_url), this, m_dest_folder));
        m_downloads.back()->start();

        wxVector<wxVariant> fields;
        fields.push_back(wxVariant(std::to_wstring(id)));
        fields.push_back(wxVariant(m_downloads.back()->get_filename()));
        fields.push_back(wxVariant(0));
        fields.push_back(wxVariant("Pending"));
        dataview->AppendItem(fields);

    } else {
        log("wrong url: " + url);
    }
   
}

void DownloadFrame::log(const wxString& msg)
{
    m_log_lines++;
    wxString old_log = m_log_label->GetLabel();
    if (m_log_lines > 10) {
        size_t endline = old_log.Find('\n');
        endline = old_log.find('\n', endline + 1);
        if (endline != wxString::npos) {
            old_log = "Log:\n" + old_log.substr(endline + 1);
        }
    }
    //m_log_label->SetLabel(old_log +"\n"+ msg);
    //printf("%s\n",old_log + "\n" + msg);
    m_full_log += +"\n" + msg;
}

void DownloadFrame::on_progress(wxCommandEvent& event)
{
    //log(std::to_string(event.GetInt()) + ": " + event.GetString());
    dataview->SetValue(std::stoi(boost::nowide::narrow(event.GetString())), event.GetInt() - 1, 2);
    dataview->SetValue("Downloading", event.GetInt() -1, 3);

}
void DownloadFrame::on_error(wxCommandEvent& event)
{
    //log(std::to_string(event.GetInt()) + ": " + event.GetString());
    //SetStatusText(event.GetString().c_str());
    dataview->SetValue("Error", event.GetInt() - 1, 3);
}
void DownloadFrame::on_complete(wxCommandEvent& event)
{
    dataview->SetValue("Done", event.GetInt() - 1, 3);
    m_instance_send->start_with_path(event.GetString());

    
}
void DownloadFrame::handle_message(const wxString& msg)
{
    log("recieved: " +  msg);
    start_download(msg);
}


//wxIMPLEMENT_APP_NO_MAIN(MyApp);


//int main()
//{
//    wxEntry();
//    return 0;
//}
//int APIENTRY WinMain(HINSTANCE /* hInstance */, HINSTANCE /* hPrevInstance */, PWSTR /* lpCmdLine */, int /* nCmdShow */)
//{
//    wxEntry();
//    return 0;
//}





//
//// ----------------------------------------------------------------------------
//// TextRenderer
//// ----------------------------------------------------------------------------
//
//bool TextRenderer::SetValue(const wxVariant& value)
//{
//    m_value = value.GetString();
//    return true;
//}
//
//bool TextRenderer::GetValue(wxVariant& value) const
//{
//    value = m_value;
//    return false;
//}
//
//bool TextRenderer::Render(wxRect rect, wxDC* dc, int state)
//{
//
//    RenderText(m_value, 0, rect, dc, state);
//
////
////#ifdef _WIN32
////    // workaround for Windows DarkMode : Don't respect to the state & wxDATAVIEW_CELL_SELECTED to avoid update of the text color
////    RenderText(m_value, 0, rect, dc, state & wxDATAVIEW_CELL_SELECTED ? 0 : state);
////#else
////    RenderText(m_value, 0, rect, dc, state);
////#endif
//
//    return true;
//}
//
//wxSize TextRenderer::GetSize() const
//{
//    return GetTextExtent(m_value);
//}
//















}

wxIMPLEMENT_APP(Downloader::DownloadApp);