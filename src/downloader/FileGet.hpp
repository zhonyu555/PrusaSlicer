#ifndef slic3r_FileGet_hpp_
#define slic3r_FileGet_hpp_

#include "FromSlicer/Http.hpp"

#include <memory>
#include <string>
#include <wx/event.h>
#include <wx/frame.h>
#include <boost/filesystem.hpp>

namespace Downloader {
class FileGet : public std::enable_shared_from_this<FileGet> {
private:
	struct priv;
public:
	FileGet(int ID, std::string url, wxEvtHandler* evt_handler,const boost::filesystem::path& dest_folder);
	FileGet(FileGet&& other);
	~FileGet();

	std::shared_ptr<FileGet> get();
	const int get_ID() const { return m_ID; }
private:
	std::unique_ptr<priv> p;
	const int m_ID;
};
wxDECLARE_EVENT(EVT_FILE_COMPLETE, wxCommandEvent);
wxDECLARE_EVENT(EVT_FILE_PROGRESS, wxCommandEvent);
wxDECLARE_EVENT(EVT_FILE_ERROR, wxCommandEvent);

}
#endif
