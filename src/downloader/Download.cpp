#include "Download.hpp"

namespace Downloader{

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
}