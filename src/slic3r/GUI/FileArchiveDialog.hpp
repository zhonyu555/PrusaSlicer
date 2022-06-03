#ifndef slic3r_GUI_FileArchiveDialog_hpp_
#define slic3r_GUI_FileArchiveDialog_hpp_

#include "GUI_Utils.hpp"
#include <wx/wx.h>


namespace Slic3r {
namespace GUI {

class FileArchiveDialog : public DPIDialog
{
public:
    FileArchiveDialog(const std::vector<std::string>& files);
        
protected:
    void on_dpi_changed(const wxRect& suggested_rect) override;


};







} // namespace GU
} // namespace Slic3r
#endif //  slic3r_GUI_FileArchiveDialog_hpp_