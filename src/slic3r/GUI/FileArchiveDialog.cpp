#include "FileArchiveDialog.hpp"

#include "I18N.hpp"
#include "GUI_App.hpp"
#include "GUI.hpp"
#include "MainFrame.hpp"

namespace Slic3r {
namespace GUI {

FileArchiveDialog::FileArchiveDialog(const std::vector<std::string>& files)
    : DPIDialog(static_cast<wxWindow*>(wxGetApp().mainframe), wxID_ANY, _(L("Archive preview")), wxDefaultPosition,
        wxSize(45 * wxGetApp().em_unit(), 40 * wxGetApp().em_unit()),
        wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX)
{
}

void FileArchiveDialog::on_dpi_changed(const wxRect& suggested_rect)
{
}

} // namespace GUI
} // namespace Slic3r