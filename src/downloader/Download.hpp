#ifndef slic3r_Download_hpp_
#define slic3r_Download_hpp_

#include "FileGet.hpp"

#include <wx/wx.h>

namespace Downloader {

enum DownloadState
{
    DownloadPending = 0,
    DownloadOngoing,
    DownloadStopped,
    DownloadDone,
    DownloadError,
    DownloadPaused
};

class Download { 
public:
    Download(int ID, std::string url, wxEvtHandler* evt_handler,const boost::filesystem::path& dest_folder);
    void start();
    void cancel();
    void pause();
    void resume();

    int get_id() const { return m_id; }
    boost::filesystem::path get_final_path() const { return m_final_path; }
    std::string get_filename() const { return m_filename; }
    DownloadState get_state() const { return m_state; }
    void set_state(DownloadState state) { m_state = state; }
private: 
    const int m_id;
    std::string m_filename;
    boost::filesystem::path m_final_path;
    std::shared_ptr<FileGet> m_file_get;
    DownloadState m_state { DownloadState::DownloadPending };
};
}

#endif