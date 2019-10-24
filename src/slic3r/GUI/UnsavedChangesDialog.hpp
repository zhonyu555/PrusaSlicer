#ifndef slic3r_GUI_UnsavedChangesDialog_hpp_
#define slic3r_GUI_UnsavedChangesDialog_hpp_

#include <wx/wx.h>
#include <wx/checkbox.h>
#include "GUI_App.hpp"
#include "Tab.hpp"
#include "PresetBundle.hpp"
#include "libslic3r/Config.hpp"

namespace Slic3r {
	namespace GUI {
		struct def_opt_pair {
			const ConfigOptionDef* def = NULL;
			const ConfigOption* old_opt = NULL;
			const ConfigOption* new_opt = NULL;
			t_config_option_key key;

			bool operator <(const def_opt_pair& b)
			{
				return this->def->category < b.def->category;
			}
		};

		struct dirty_opt {
			def_opt_pair val;
			wxCheckBox* checkbox;

			dirty_opt(def_opt_pair val, wxCheckBox* checkbox) {
				this->val = val;
				this->checkbox = checkbox;
			}
		};

		struct dirty_opts_node {
			std::string name = "";
			Tab* tab = NULL;
			std::vector<dirty_opts_node> childs;
			std::vector<dirty_opt> opts;
		};

		class UnsavedChangesDialog : public wxDialog
		{
		public:
			UnsavedChangesDialog(wxWindow* parent, GUI_App* app, const wxString& header, const wxString& caption = wxMessageBoxCaptionStr, long style = wxOK | wxCENTRE, const wxPoint& pos = wxDefaultPosition);

		private:
			GUI_App* m_app;
			wxScrolledWindow* m_scroller;
			dirty_opts_node m_dirty_tabs_tree;

			wxWindow* buildScrollWindow(wxString& dirty_tabs);
			void add_dirty_options(Tab* tab, wxWindow* parent, wxBoxSizer* sizer);
			std::string getTooltipText(const ConfigOptionDef& def);
			wxBoxSizer* buildYesNoBtns();
			dirty_opts_node& buildNode(dirty_opts_node& treeParent, std::string name, Tab* tab = NULL);
			wxCheckBox* buildCheckbox(wxWindow* parent, const wxString& label, dirty_opts_node& parent_node, wxSize size = wxDefaultSize);
		};
	}
}

#endif