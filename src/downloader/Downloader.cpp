#include "Downloader.hpp"
#include "FileGet.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <boost/nowide/convert.hpp>
#include <wx/event.h>

namespace Downloader {

enum
{
    ID_Hello = 1,
};


wxBEGIN_EVENT_TABLE(DownloadFrame, wxFrame)
EVT_MENU(ID_Hello, DownloadFrame::OnHello)
EVT_MENU(wxID_EXIT, DownloadFrame::OnExit)
EVT_MENU(wxID_ABOUT, DownloadFrame::OnAbout)
wxEND_EVENT_TABLE()


bool DownloadApp::OnInit()
{
    DownloadFrame* frame = new DownloadFrame("Hello World", wxPoint(50, 50), wxSize(450, 340));
    frame->Show(true);
    return true;
}
DownloadFrame::DownloadFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
    : wxFrame(NULL, wxID_ANY, title, pos, size)
{
    wxMenu* menuFile = new wxMenu;
    menuFile->Append(ID_Hello, "&Hello...\tCtrl-H",
        "Help string shown in status bar for this menu item");
    menuFile->AppendSeparator();
    menuFile->Append(wxID_EXIT);
    wxMenu* menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT);
    wxMenuBar* menuBar = new wxMenuBar;
    menuBar->Append(menuFile, "&File");
    menuBar->Append(menuHelp, "&Help");
    SetMenuBar(menuBar);
    CreateStatusBar();
    SetStatusText("Welcome to wxWidgets!");

    m_log_label = new wxStaticText(this, wxID_ANY, "Log:");
    auto* sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(m_log_label, wxEXPAND);
    SetSizer(sizer);

  //  Bind(EVT_FILE_COMPLETE, &on_complete, this);
    Bind(EVT_FILE_COMPLETE, &DownloadFrame::on_complete, this);
    Bind(EVT_FILE_PROGRESS, &DownloadFrame::on_progress, this);
    Bind(EVT_FILE_ERROR, &DownloadFrame::on_error, this);
}
void DownloadFrame::OnExit(wxCommandEvent& event)
{
    Close(true);
}
void DownloadFrame::OnAbout(wxCommandEvent& event)
{
    wxMessageBox("This is a wxWidgets' Hello world sample",
        "About Hello World", wxOK | wxICON_INFORMATION);
}
void DownloadFrame::OnHello(wxCommandEvent& event)
{

    std::string test_url = "https%3A%2F%2Fmedia.printables.com%2Fmedia%2Fprints%2F152208%2Fstls%2F1431590_8b8287b3-03b1-4cbe-82d0-268a0affa171%2Ff1_logo.stl";
    std::shared_ptr<FileGet> file_get = FileGet(get_next_id(), test_url, this, boost::filesystem::path("C:\\Users\\User\\Downloads")) .get();
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
    m_log_label->SetLabel(old_log +"\n"+ msg);
    m_full_log += +"\n" + msg;
}

void DownloadFrame::on_progress(wxCommandEvent& event)
{
    log(std::to_string(event.GetInt()) + ": " + event.GetString());
    //SetStatusText("Progress: " + std::to_string(event.GetInt()));
}
void DownloadFrame::on_error(wxCommandEvent& event)
{
    log(std::to_string(event.GetInt()) + ": " + event.GetString());
    //SetStatusText(event.GetString().c_str());
}
void DownloadFrame::on_complete(wxCommandEvent& event)
{
    log(std::to_string(event.GetInt()) + ": Download complete " + event.GetString());
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

}

wxIMPLEMENT_APP(Downloader::DownloadApp);