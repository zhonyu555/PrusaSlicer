#include "Downloader.hpp"
#include "GUI_App.hpp"
#include "NotificationManager.hpp"

namespace Slic3r {
namespace GUI {

namespace {
std::string filename_from_url(const std::string& url)
{
	// TODO: can it be done with curl?
	size_t slash = url.find_last_of("/");
	if (slash == std::string::npos && slash != url.size() - 1)
		return std::string();
	return url.substr(slash + 1, url.size() - slash + 1);
}
}

Download::Download(int ID, std::string url, wxEvtHandler* evt_handler, const boost::filesystem::path& dest_folder)
    : m_id(ID)
	, m_filename(filename_from_url(url))
{
	assert(boost::filesystem::is_directory(dest_folder));
	m_final_path = dest_folder / m_filename;
    m_file_get = std::make_shared<FileGet>(ID, std::move(url), m_filename, evt_handler, dest_folder);
}

void Download::start()
{
	m_state = DownloadState::DownloadOngoing;
	m_file_get->get();
}
void Download::cancel()
{
	m_state = DownloadState::DownloadStopped;
	m_file_get->cancel();
}
void Download::pause()
{
	assert(m_state == DownloadState::DownloadOngoing);
	m_state = DownloadState::DownloadPaused;
	m_file_get->pause();
}
void Download::resume()
{
	assert(m_state == DownloadState::DownloadPaused);
	m_state = DownloadState::DownloadOngoing;
	m_file_get->resume();
}


Downloader::Downloader()
	: wxEvtHandler()
{
	Bind(EVT_DWNLDR_FILE_COMPLETE, [](const wxCommandEvent& evt) {});
	Bind(EVT_DWNLDR_FILE_PROGRESS, [](const wxCommandEvent& evt) {});
	Bind(EVT_DWNLDR_FILE_ERROR, [](const wxCommandEvent& evt) {});
	Bind(EVT_DWNLDR_FILE_NAME_CHANGE, [](const wxCommandEvent& evt) {});

	Bind(EVT_DWNLDR_FILE_COMPLETE, &Downloader::on_complete, this);
	Bind(EVT_DWNLDR_FILE_PROGRESS, &Downloader::on_progress, this);
	Bind(EVT_DWNLDR_FILE_ERROR, &Downloader::on_error, this);
	Bind(EVT_DWNLDR_FILE_NAME_CHANGE, &Downloader::on_name_change, this);
}

void Downloader::start_download(const std::string& full_url)
{
	assert(m_initialized);
	// prusaslicer://open/?file=https%3A%2F%2Fmedia.prusaverse.coex.cz%2Fmedia%2Fprints%2F78695%2Fstls%2F847188_185abf43-1439-4466-8777-3f625719498f%2Farm-upper-arm.stl
    if (full_url.rfind("prusaslicer://open/?file=", 0) != 0) {
		BOOST_LOG_TRIVIAL(error) << "Could not start download due to wrong url: " << full_url << "	" << full_url.rfind("prusaslicer://open?file=", 0);
		return;
	}
    size_t id = get_next_id();
    std::string escaped_url = FileGet::escape_url(full_url.substr(25));
	std::string text(escaped_url);
    m_downloads.emplace_back(std::make_unique<Download>(id, std::move(escaped_url), this, m_dest_folder));
	NotificationManager* ntf_mngr = wxGetApp().notification_manager();
	ntf_mngr->push_download_URL_progress_notification(id, text, std::bind(&Downloader::cancel_callback, this));
	m_downloads.back()->start();
	BOOST_LOG_TRIVIAL(error) << "started download";
}

void Downloader::on_progress(wxCommandEvent& event)
{
	size_t id = event.GetInt();
	float percent = (float)std::stoi(boost::nowide::narrow(event.GetString())) / 100.f;
	BOOST_LOG_TRIVIAL(error) << "progress " << id << ": " << percent;
	NotificationManager* ntf_mngr = wxGetApp().notification_manager();
	ntf_mngr->set_download_URL_progress(id, percent);
}
void Downloader::on_error(wxCommandEvent& event)
{
    set_download_state(event.GetInt(), DownloadState::DownloadError);   
}
void Downloader::on_complete(wxCommandEvent& event)
{
    set_download_state(event.GetInt(), DownloadState::DownloadDone);
}
bool Downloader::cancel_callback()
{
	//TODO
	return true;
}
void Downloader::on_name_change(wxCommandEvent& event)
{
   
}

void Downloader::set_download_state(size_t id, DownloadState state)
{
    for (size_t i = 0; i < m_downloads.size(); ++i) {
        if (m_downloads[i]->get_id() == id) {
            m_downloads[i]->set_state(state);
            return;
        }
    }
}

}
}