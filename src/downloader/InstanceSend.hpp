#ifndef slic3r_InstanceSend_hpp_
#define slic3r_InstanceSend_hpp_

#include <boost/filesystem/path.hpp>
#include <wx/string.h>

namespace Downloader {
class SlicerSend
{
public:
    bool get_instance_exists() const;
    bool send_path(const wxString& path) const;
    bool start_with_path(const wxString& path) const;
    bool start_or_send(const wxString& path) const;
};

class DownloaderSend
{
public:
    bool get_instance_exists() const;
    bool send_url(const wxString& url) const;
};
}
#endif