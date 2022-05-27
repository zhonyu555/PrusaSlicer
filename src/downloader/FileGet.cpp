#include "FileGet.hpp"


#include <thread>
#include <curl/curl.h>
#include <boost/nowide/fstream.hpp>


namespace Downloader {


std::string FileGet::escape_url(const std::string& unescaped)
{
	std::string ret_val;
	CURL* curl = curl_easy_init();
	if (curl) {
		int decodelen;
		char* decoded = curl_easy_unescape(curl, unescaped.c_str(), unescaped.size(), &decodelen);
		if (decoded) {
			ret_val = std::string(decoded);
			curl_free(decoded);
		}
		curl_easy_cleanup(curl);
	}
	return ret_val;
}
namespace {
unsigned get_current_pid()
{
#ifdef WIN32
	return GetCurrentProcessId();
#else
	return ::getpid();
#endif
}
}

// int = DOWNLOAD ID; string = file path
wxDEFINE_EVENT(EVT_FILE_COMPLETE, wxCommandEvent);
// int = DOWNLOAD ID; string = error msg
wxDEFINE_EVENT(EVT_FILE_ERROR, wxCommandEvent);
// int = DOWNLOAD ID; string = progress percent
wxDEFINE_EVENT(EVT_FILE_PROGRESS, wxCommandEvent);

struct FileGet::priv
{
	const int m_id;
	std::string m_url;
	std::string m_filename;
	std::thread m_io_thread;
	wxEvtHandler* m_evt_handler;
	boost::filesystem::path m_dest_folder;
	std::atomic_bool m_cancel = false;
	priv(int ID, std::string&& url, const std::string& filename, wxEvtHandler* evt_handler, const boost::filesystem::path& dest_folder);

	void get_perform();
};

FileGet::priv::priv(int ID, std::string&& url, const std::string& filename, wxEvtHandler* evt_handler, const boost::filesystem::path& dest_folder)
	: m_id(ID)
	, m_url(std::move(url))
	, m_filename(filename)
	, m_evt_handler(evt_handler)
	, m_dest_folder(dest_folder)
{
}

void FileGet::priv::get_perform()
{
	assert(m_evt_handler);
	assert(!m_url.empty());
	assert(!m_url.empty());
	assert(!m_filename.empty());
	assert(boost::filesystem::is_directory(m_dest_folder));

	Downloader::Http::get(m_url)
		//.size_limit(size_limit)
		.on_progress([&](Downloader::Http::Progress progress, bool& cancel) {
			if (m_cancel) {
				cancel = true;
				return;
				// TODO: send canceled event?
			}
			wxCommandEvent* evt = new wxCommandEvent(EVT_FILE_PROGRESS);
			if (progress.dlnow == 0)
				evt->SetString("0");
			else
				evt->SetString(std::to_string(progress.dlnow * 100 / progress.dltotal));
			evt->SetInt(m_id);
			m_evt_handler->QueueEvent(evt);
		})
		.on_error([&](std::string body, std::string error, unsigned http_status) {
			wxCommandEvent* evt = new wxCommandEvent(EVT_FILE_ERROR);
			evt->SetString(error);
			evt->SetInt(m_id);
			m_evt_handler->QueueEvent(evt);
		})
		.on_complete([&](std::string body, unsigned /* http_status */) {
			
			size_t body_size = body.size();
			// TODO:
			//if (body_size != expected_size) {
			//	return;
			//}
			
			boost::filesystem::path dest_path = m_dest_folder / m_filename;
			std::string extension = boost::filesystem::extension(dest_path);
			std::string just_filename = m_filename.substr(0, m_filename.size() - extension.size());
			std::string final_filename = just_filename;

			size_t version = 0;
			while (boost::filesystem::exists(m_dest_folder / (final_filename + extension)))
			{
				++version;
				final_filename = just_filename + "("+std::to_string(version)+")";
			}
			m_filename = final_filename + extension;

			boost::filesystem::path tmp_path = m_dest_folder / (m_filename + "." + std::to_string(get_current_pid()) +  ".download");
			dest_path = m_dest_folder / m_filename;
			try
			{
				boost::nowide::fstream file(tmp_path.string(), std::ios::out | std::ios::binary | std::ios::trunc);
				file.write(body.c_str(), body.size());
				file.close();
				boost::filesystem::rename(tmp_path, dest_path);
			}
			catch (const std::exception&)
			{
				//TODO: report?
				//error_message = GUI::format("Failed to write and move %1% to %2%", tmp_path, dest_path);
				wxCommandEvent* evt = new wxCommandEvent(EVT_FILE_ERROR);
				evt->SetString("Failed to write and move.");
				evt->SetInt(m_id);
				m_evt_handler->QueueEvent(evt);
				return;
			}

			wxCommandEvent* evt = new wxCommandEvent(EVT_FILE_COMPLETE);
			evt->SetString(dest_path.string());
			evt->SetInt(m_id);
			m_evt_handler->QueueEvent(evt);
		})
		.perform_sync();

}

FileGet::FileGet(int ID, std::string url, const std::string& filename, wxEvtHandler* evt_handler, const boost::filesystem::path& dest_folder)
	: p(new priv(ID, std::move(url), filename, evt_handler, dest_folder))
{}

FileGet::FileGet(FileGet&& other) : p(std::move(other.p)) {}

FileGet::~FileGet()
{
	if (p && p->m_io_thread.joinable()) {
		p->m_io_thread.detach();
	}
}

void FileGet::get()
{
	if (p) {
		auto io_thread = std::thread([&priv = p]() {
			priv->get_perform();
			});
		p->m_io_thread = std::move(io_thread);
	}
}

void FileGet::cancel()
{
	if(p){
		p->m_cancel = true;
	}
}

}
