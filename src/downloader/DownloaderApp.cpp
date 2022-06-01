#include "DownloaderApp.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <boost/nowide/convert.hpp>
#include <wx/event.h>
#include <wx/cmdline.h>

namespace Downloader {


namespace{
void open_folder(const wxString& widepath)
{
    // Code taken from desktop_open_datadir_folder()

    // Execute command to open a file explorer, platform dependent.
    // FIXME: The const_casts aren't needed in wxWidgets 3.1, remove them when we upgrade.

#ifdef _WIN32
    //const wxString widepath = wxString::FromUTF8(path.c_str());
    const wchar_t* argv[] = { L"explorer", widepath.GetData(), nullptr };
    ::wxExecute(const_cast<wchar_t**>(argv), wxEXEC_ASYNC, nullptr);
#elif __APPLE__
    std::string path = boost::nowide::narrow(widepath);
    const char* argv[] = { "open", path.data(), nullptr };
    ::wxExecute(const_cast<char**>(argv), wxEXEC_ASYNC, nullptr);
#else
    std::string path = boost::nowide::narrow(widepath);
    const char* argv[] = { "xdg-open", path.data(), nullptr };

    // Check if we're running in an AppImage container, if so, we need to remove AppImage's env vars,
    // because they may mess up the environment expected by the file manager.
    // Mostly this is about LD_LIBRARY_PATH, but we remove a few more too for good measure.
    if (wxGetEnv("APPIMAGE", nullptr)) {
        // We're running from AppImage
        wxEnvVariableHashMap env_vars;
        wxGetEnvMap(&env_vars);

        env_vars.erase("APPIMAGE");
        env_vars.erase("APPDIR");
        env_vars.erase("LD_LIBRARY_PATH");
        env_vars.erase("LD_PRELOAD");
        env_vars.erase("UNION_PRELOAD");

        wxExecuteEnv exec_env;
        exec_env.env = std::move(env_vars);

        wxString owd;
        if (wxGetEnv("OWD", &owd)) {
            // This is the original work directory from which the AppImage image was run,
            // set it as CWD for the child process:
            exec_env.cwd = std::move(owd);
        }

        ::wxExecute(const_cast<char**>(argv), wxEXEC_ASYNC, nullptr, &exec_env);
    }
    else {
        // Looks like we're NOT running from AppImage, we'll make no changes to the environment.
        ::wxExecute(const_cast<char**>(argv), wxEXEC_ASYNC, nullptr, nullptr);
    }
#endif
}
} // namespace

bool DownloadApp::OnInit()
{
    m_dwnldr_send = std::make_unique<DownloaderSend>();
   

    if (m_dwnldr_send->get_instance_exists()) {
        m_other_exists = true;
        m_frame = new DownloadFrame("Downloader", wxPoint(50, 50), wxSize(0,0));
        return wxApp::OnInit();
    } 
    m_frame = new DownloadFrame("Downloader", wxPoint(50, 50), wxSize(450, 340));
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

        //m_frame->start_download("open?file=https%3A%2F%2Fmedia.printables.com%2Fmedia%2Fprints%2F152208%2Fstls%2F1431590_8b8287b3-03b1-4cbe-82d0-268a0affa171%2Ff1_logo.stl");

        return wxApp::OnCmdLineParsed(parser);
    } 
}



DownloadFrame::DownloadFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
    : wxFrame(NULL, wxID_ANY, title, pos, size)
    , m_slicer_send(std::make_unique<SlicerSend>())
{

    auto * data_sizer = new wxBoxSizer(wxVERTICAL);

    m_dataview = new wxDataViewListCtrl(this, wxID_ANY);

   /* m_dataview->AppendColumn(new wxDataViewColumn("ID", new TextRenderer(), 0, 100, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE));
    m_dataview->AppendColumn(new wxDataViewColumn("Filename", new TextRenderer(), 1, 100, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE));
    m_dataview->AppendProgressColumn("Progress", wxDATAVIEW_CELL_INERT, 100, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    m_dataview->AppendColumn(new wxDataViewColumn("status", new TextRenderer(), 2, 100, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE));*/


    m_dataview->AppendTextColumn("ID", wxDATAVIEW_CELL_INERT, 30, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    m_dataview->AppendTextColumn("Filename", wxDATAVIEW_CELL_INERT, 100, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    m_dataview->AppendProgressColumn("Progress", wxDATAVIEW_CELL_INERT, 100, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    m_dataview->AppendTextColumn("status", wxDATAVIEW_CELL_INERT, 100, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);

    data_sizer->Add(m_dataview, 1, wxEXPAND | wxBOTTOM);

    m_dest_folder = boost::filesystem::path("C:\\Users\\User\\Downloads");

    //m_log_label = new wxStaticText(this, wxID_ANY, "Log:");
    
    ////sizer->Add(m_log_label, wxEXPAND);
    //

    

    wxBoxSizer* btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton* btn_run = new wxButton(this, wxID_OK, "Run");
    btn_run->Bind(wxEVT_BUTTON, [this](wxCommandEvent& evt) { on_open_in_slicer(evt); });
    btn_sizer->Add(btn_run, 0, wxLEFT | wxALIGN_CENTER_VERTICAL);

    wxButton* btn_run_new = new wxButton(this, wxID_EDIT, "Run new");
    btn_run_new->Bind(wxEVT_BUTTON, [this](wxCommandEvent& evt) { on_open_in_new_slicer(evt); });
    btn_sizer->Add(btn_run_new, 0, wxLEFT | wxALIGN_CENTER_VERTICAL);

    wxButton* btn_folder = new wxButton(this, wxID_EDIT, "Open");
    btn_folder->Bind(wxEVT_BUTTON, [this](wxCommandEvent& evt) { on_open_in_explorer(evt); });
    btn_sizer->Add(btn_folder, 0, wxLEFT | wxALIGN_CENTER_VERTICAL);
    
    wxButton* btn_cancel = new wxButton(this, wxID_EDIT, "Cancel");
    btn_cancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent& evt) { on_cancel_button(evt); });
    btn_sizer->Add(btn_cancel, 0, wxLEFT | wxALIGN_CENTER_VERTICAL);

    wxButton* btn_pause = new wxButton(this, wxID_EDIT, "Pause");
    btn_pause->Bind(wxEVT_BUTTON, [this](wxCommandEvent& evt) { on_pause_button(evt); });
    btn_sizer->Add(btn_pause, 0, wxLEFT | wxALIGN_CENTER_VERTICAL);


    wxButton* btn_resume = new wxButton(this, wxID_EDIT, "Resume");
    btn_resume->Bind(wxEVT_BUTTON, [this](wxCommandEvent& evt) { on_resume_button(evt); });
    btn_sizer->Add(btn_resume, 0, wxLEFT | wxALIGN_CENTER_VERTICAL);

    //main_sizer->Add(data_sizer);
    //main_sizer->Add(btn_sizer);
   
    data_sizer->Add(btn_sizer, 0, wxEXPAND);
    SetSizer(data_sizer);

    Bind(EVT_FILE_COMPLETE, &DownloadFrame::on_complete, this);
    Bind(EVT_FILE_PROGRESS, &DownloadFrame::on_progress, this);
    Bind(EVT_FILE_ERROR, &DownloadFrame::on_error, this);
    Bind(EVT_FILE_NAME_CHANGE, &DownloadFrame::on_name_change, this);
}

void DownloadFrame::start_download(wxString url)
{
//    prusaslicer://open?file=https%3A%2F%2Fmedia.printables.com%2Fmedia%2Fprints%2F152208%2Fstls%2F1431590_8b8287b3-03b1-4cbe-82d0-268a0affa171%2Ff1_logo.stl
    if (url.starts_with("open?file=")) {
        int id = get_next_id();
        std::string escaped_url = FileGet::escape_url(boost::nowide::narrow(url.substr(10)));
        //log(std::to_string(id) + ": start " + escaped_url);
        escaped_url = "https://media.printables.com/media/prints/216267/gcodes/1974221_32d21613-b567-4328-8261-49b46d9dd249/01_big_trex_skull_with_threads_015mm_pla_mk3_2d.gcode";
        m_downloads.emplace_back(std::make_unique<Download>(id, std::move(escaped_url), this, m_dest_folder));
        //

        wxVector<wxVariant> fields;
        fields.push_back(wxVariant(std::to_wstring(id)));
        fields.push_back(wxVariant(m_downloads.back()->get_filename()));
        fields.push_back(wxVariant(0));
        fields.push_back(wxVariant("Pending"));
        m_dataview->AppendItem(fields);

        bool can_start = true;
        for (size_t i = 0; i < m_downloads.size(); i++) {
            if (m_downloads[i]->get_state() == DownloadState::DownloadOngoing) {
                //can_start = false;
                break;
            }
        }
        if (can_start)
            m_downloads.back()->start();

    } else {
        //log("wrong url: " + url);
    }
   
}

void DownloadFrame::log(const wxString& msg)
{
    //m_log_lines++;
    //wxString old_log = m_log_label->GetLabel();
    //if (m_log_lines > 10) {
    //    size_t endline = old_log.Find('\n');
    //    endline = old_log.find('\n', endline + 1);
    //    if (endline != wxString::npos) {
    //        old_log = "Log:\n" + old_log.substr(endline + 1);
    //    }
    //}
    ////m_log_label->SetLabel(old_log +"\n"+ msg);
    ////printf("%s\n",old_log + "\n" + msg);
    //m_full_log += +"\n" + msg;
}

void DownloadFrame::on_progress(wxCommandEvent& event)
{
   
    for (size_t i = 0; i < m_dataview->GetItemCount(); i++) {
        int id = std::stoi(boost::nowide::narrow(m_dataview->GetTextValue(i, 0)));
        if (id == event.GetInt()) {
            if (!is_in_state(id, DownloadState::DownloadOngoing))
                return;
            m_dataview->SetValue(std::stoi(boost::nowide::narrow(event.GetString())), i, 2);
            m_dataview->SetValue("Downloading", i, 3);
            return;
        }
    }
}
void DownloadFrame::on_error(wxCommandEvent& event)
{
    set_download_state(event.GetInt(), DownloadState::DownloadError);

    for (size_t i = 0; i < m_dataview->GetItemCount(); i++) {
        int id = std::stoi(boost::nowide::narrow(m_dataview->GetTextValue(i, 0)));
        if (id == event.GetInt()) {
            m_dataview->SetValue("Error - " + event.GetString(), i, 3);
            return;
        }
    }
}
void DownloadFrame::on_complete(wxCommandEvent& event)
{
    set_download_state(event.GetInt(), DownloadState::DownloadDone);

    for (size_t i = 0; i < m_dataview->GetItemCount(); i++) {
        int id = std::stoi(boost::nowide::narrow(m_dataview->GetTextValue(i, 0)));
        if (id == event.GetInt()) {
            m_dataview->SetValue("Done", i, 3);
            return;
        }
    }

    start_next();
}
void DownloadFrame::on_name_change(wxCommandEvent& event)
{
    for (size_t i = 0; i < m_dataview->GetItemCount(); i++) {
        int id = std::stoi(boost::nowide::narrow(m_dataview->GetTextValue(i, 0)));
        if (id == event.GetInt())  {
            m_dataview->SetValue(event.GetString(), i, 1);
            return;
        }
    }
}
void DownloadFrame::start_next()
{
    for (size_t i = 0; i < m_downloads.size(); i++) {
        if (m_downloads[i]->get_state() == DownloadState::DownloadPending) {
            m_downloads[i]->start();
            break;
        }
    }
}

void DownloadFrame::on_open_in_slicer(wxCommandEvent& event)
{
    int selected = m_dataview->GetSelectedRow();
    if (selected == wxNOT_FOUND) { return; }
    int id = std::stoi(boost::nowide::narrow(m_dataview->GetTextValue(selected,0)));
 
    if (is_in_state(id, DownloadState::DownloadDone)) {
        m_slicer_send->start_or_send(get_path_of(id));
    }
}
void DownloadFrame::on_open_in_new_slicer(wxCommandEvent& event)
{
    int selected = m_dataview->GetSelectedRow();
    if (selected == wxNOT_FOUND) { return; }
    int id = std::stoi(boost::nowide::narrow(m_dataview->GetTextValue(selected, 0)));

    if (is_in_state(id, DownloadState::DownloadDone)) {
        m_slicer_send->start_with_path(get_path_of(id));
    }
}
void DownloadFrame::on_open_in_explorer(wxCommandEvent& event)
{
    int selected = m_dataview->GetSelectedRow();
    if (selected == wxNOT_FOUND) { return; }
    int id = std::stoi(boost::nowide::narrow(m_dataview->GetTextValue(selected, 0)));

    if (is_in_state(id, DownloadState::DownloadDone)) {
        open_folder(get_folder_path_of(id));
    }
}

void DownloadFrame::on_cancel_button(wxCommandEvent& event)
{
    int selected = m_dataview->GetSelectedRow();
    if (selected == wxNOT_FOUND) { return; }
    int id = std::stoi(boost::nowide::narrow(m_dataview->GetTextValue(selected, 0)));

    if(cancel_download(id))
        m_dataview->SetValue("Canceled", selected, 3);
    if (delete_download(id))
        m_dataview->DeleteItem(selected);
}

void DownloadFrame::on_pause_button(wxCommandEvent& event)
{
    int selected = m_dataview->GetSelectedRow();
    if (selected == wxNOT_FOUND) { return; }
    int id = std::stoi(boost::nowide::narrow(m_dataview->GetTextValue(selected, 0)));

    if (pause_download(id))
        m_dataview->SetValue("Paused", selected, 3);
}

void DownloadFrame::on_resume_button(wxCommandEvent& event)
{
    int selected = m_dataview->GetSelectedRow();
    if (selected == wxNOT_FOUND) { return; }
    int id = std::stoi(boost::nowide::narrow(m_dataview->GetTextValue(selected, 0)));

    if (resume_download(id))
        m_dataview->SetValue("Paused", selected, 3);
}

void DownloadFrame::set_download_state(int id, DownloadState state)
{
    for( size_t i = 0; i < m_downloads.size(); ++i) {
        if (m_downloads[i]->get_id() == id) {
            m_downloads[i]->set_state(state);
            return;
        }
    }
}

void DownloadFrame::update_state_labels()
{
    for (size_t i = 0; i < m_dataview->GetItemCount(); i++) {
        int id = std::stoi(boost::nowide::narrow(m_dataview->GetTextValue(i, 0)));
        m_dataview->SetValue(c_state_labels.at(get_download_state(id)), i, 1);
    }
}

DownloadState DownloadFrame::get_download_state(int id) const
{
    for (size_t i = 0; i < m_downloads.size(); ++i) {
        if (m_downloads[i]->get_id() == id) {
            return m_downloads[i]->get_state();
        }
    }
}

bool DownloadFrame::is_in_state(int id, DownloadState state) const
{
    for (size_t i = 0; i < m_downloads.size(); ++i) {
        if (m_downloads[i]->get_id() == id) {
            return m_downloads[i]->get_state() == state;
        }
    }
}

bool DownloadFrame::cancel_download(int id)
{
    for (size_t i = 0; i < m_downloads.size(); ++i) {
        if (m_downloads[i]->get_id() == id) {
            if (m_downloads[i]->get_state() == DownloadState::DownloadOngoing) {
                m_downloads[i]->cancel();
                return true;
            }
            return false;
        }
    }
    return false;
}

bool DownloadFrame::pause_download(int id)
{
    for (size_t i = 0; i < m_downloads.size(); ++i) {
        if (m_downloads[i]->get_id() == id) {
            if (m_downloads[i]->get_state() == DownloadState::DownloadOngoing) {
                m_downloads[i]->pause();
                return true;
            }
            return false;
        }
    }
    return false;
}

bool DownloadFrame::resume_download(int id)
{
    for (size_t i = 0; i < m_downloads.size(); ++i) {
        if (m_downloads[i]->get_id() == id) {
            if (m_downloads[i]->get_state() == DownloadState::DownloadPaused) {
                m_downloads[i]->resume();
                return true;
            }
            return false;
        }
    }
    return false;
}

bool DownloadFrame::delete_download(int id)
{
    for (size_t i = 0; i < m_downloads.size(); ++i) {
        if (m_downloads[i]->get_id() == id) {
            m_downloads.erase(m_downloads.begin() + i);
            return true;
        }
    }
    return false;
}

wxString DownloadFrame::get_path_of(int id) const
{
    for (size_t i = 0; i < m_downloads.size(); ++i) {
        if (m_downloads[i]->get_id() == id) {
            return m_downloads[i]->get_final_path().string();
        }
    }
}

wxString DownloadFrame::get_folder_path_of(int id) const
{
    for (size_t i = 0; i < m_downloads.size(); ++i) {
        if (m_downloads[i]->get_id() == id) {
            return m_downloads[i]->get_final_path().parent_path().string();
        }
    }
}

void DownloadFrame::handle_message(const wxString& msg)
{
    //log("recieved: " +  msg);
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