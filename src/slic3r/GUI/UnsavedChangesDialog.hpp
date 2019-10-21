#ifndef slic3r_GUI_UnsavedChangesDialog_hpp_
#define slic3r_GUI_UnsavedChangesDialog_hpp_

#include <wx/wx.h>
#include "GUI_App.hpp"

namespace Slic3r {
	namespace GUI {
		class UnsavedChangesDialog : public wxDialog
		{
		public:
			UnsavedChangesDialog(wxWindow* parent, GUI_App* app, const wxString& header, const wxString& caption = wxMessageBoxCaptionStr, long style = wxOK | wxCENTRE, const wxPoint& pos = wxDefaultPosition);
		private:
			GUI_App* m_app;
			wxScrolledWindow* buildScrollWindow(wxString& dirty_tabs);
			void add_dirty_options(Tab* tab, wxWindow* parent, wxBoxSizer* sizer);
			std::string getTooltipText(const ConfigOptionDef& def);
		};
	}
}

#endif