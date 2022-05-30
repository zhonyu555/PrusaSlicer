#include "FileGet.hpp"


#include <thread>
#include <curl/curl.h>
#include <boost/nowide/fstream.hpp>


namespace Downloader {

const size_t DOWNLOAD_MAX_CHUNK_SIZE	= 10 * 1024 * 1024;
const size_t DOWNLOAD_SIZE_LIMIT		= 1024 * 1024 * 1024;

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
// int = DOWNLOAD ID; string = name
wxDEFINE_EVENT(EVT_FILE_NAME_CHANGE, wxCommandEvent);


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

	// open dest file
	boost::filesystem::path dest_path = m_dest_folder / m_filename;
	std::string extension = boost::filesystem::extension(dest_path);
	std::string just_filename = m_filename.substr(0, m_filename.size() - extension.size());
	std::string final_filename = just_filename;

	size_t version = 0;
	while (boost::filesystem::exists(m_dest_folder / (final_filename + extension)) || boost::filesystem::exists(m_dest_folder / (final_filename + extension + "." + std::to_string(get_current_pid()) + ".download")))
	{
		++version;
		final_filename = just_filename + "(" + std::to_string(version) + ")";
	}
	m_filename = final_filename + extension;

	boost::filesystem::path tmp_path = m_dest_folder / (m_filename + "." + std::to_string(get_current_pid()) + ".download");
	dest_path = m_dest_folder / m_filename;

	wxCommandEvent* evt = new wxCommandEvent(EVT_FILE_NAME_CHANGE);
	evt->SetString(boost::nowide::widen(m_filename));
	evt->SetInt(m_id);
	m_evt_handler->QueueEvent(evt);

	// open file for writting
	FILE* file = fopen(tmp_path.string().c_str(), "wb");
	size_t written = 0;

	Downloader::Http::get(m_url)
		.size_limit(DOWNLOAD_SIZE_LIMIT) //more?
		.on_progress([&](Downloader::Http::Progress progress, bool& cancel) {
			if (m_cancel) {
				fclose(file);
				// remove canceled file
				std::remove(tmp_path.string().c_str());
				cancel = true;
				return;
				// TODO: send canceled event?
			}			
			wxCommandEvent* evt = new wxCommandEvent(EVT_FILE_PROGRESS);
			if (progress.dlnow == 0)
				evt->SetString("0");
			else {	
				if (progress.dlnow - written > DOWNLOAD_MAX_CHUNK_SIZE || progress.dlnow == progress.dltotal) {
					try
					{
						std::string part_for_write = progress.buffer.substr(written, progress.dlnow);
						fwrite(part_for_write.c_str(), 1, part_for_write.size(), file);
					}
					catch (const std::exception&)
					{
						// fclose(file); do it?
						wxCommandEvent* evt = new wxCommandEvent(EVT_FILE_ERROR);
						evt->SetString("Failed to write progress.");
						evt->SetInt(m_id);
						m_evt_handler->QueueEvent(evt);
						cancel = true;
						return;
					}
					written = progress.dlnow;
				}
				evt->SetString(std::to_string(progress.dlnow * 100 / progress.dltotal));
			}
			evt->SetInt(m_id);
			m_evt_handler->QueueEvent(evt);
		})
		.on_error([&](std::string body, std::string error, unsigned http_status) {
			fclose(file);
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
			try
			{
				if (written < body.size())
				{
					// this code should never be entered. As there should be on_progress call after last bit downloaded.
					std::string part_for_write = body.substr(written);
					fwrite(part_for_write.c_str(), 1, part_for_write.size(), file);
				}
				fclose(file);
				boost::filesystem::rename(tmp_path, dest_path);
			}
			catch (const std::exception& e)
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
