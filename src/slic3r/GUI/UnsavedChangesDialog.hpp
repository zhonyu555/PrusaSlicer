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

			int index = -1;
			std::string ser_old_opt;
			std::string ser_new_opt;

			bool operator <(const def_opt_pair& b)
			{
				if (this->def->category == b.def->category && this->index >= 0 && b.index >= 0)
				{
					return this->index < b.index;
				}

				return this->def->category < b.def->category;
			}
		};

		struct dirty_opts_node;

		//this binds the gui line of an option and its definition and internal value together
		struct dirty_opt {
			def_opt_pair val;
			wxCheckBox* checkbox;
			wxWindow* old_win;
			wxWindow* new_win;

			dirty_opts_node* parent;

			dirty_opt(def_opt_pair val, wxCheckBox* checkbox, dirty_opts_node* parent) {
				this->val = val;
				this->checkbox = checkbox;
				this->parent = parent;
			}
		};

		//a node represents a tab or category in the scroll window
		struct dirty_opts_node {
			wxString label = "";
			wxCheckBox* checkbox;
			Tab* tab = NULL;
			std::vector<dirty_opts_node*> childs;
			std::vector<dirty_opt> opts;

			void enableChilds(bool enabled = true) {
				for (dirty_opts_node* cur_node : this->childs) {
					cur_node->checkbox->Enable(enabled);
					cur_node->enableChilds(enabled);
				}

				for (dirty_opt cur_opt : this->opts) {
					cur_opt.checkbox->Enable(enabled);
					cur_opt.old_win->Enable(enabled);
					cur_opt.new_win->Enable(enabled);
				}
			}

			void selectChilds(bool selected = true) {
				for (dirty_opts_node* cur_node : this->childs) {
					cur_node->checkbox->SetValue(selected);
					cur_node->selectChilds(selected);
				}

				for (dirty_opt cur_opt : this->opts) {
					cur_opt.checkbox->SetValue(selected);
				}

				this->enableChilds(selected);
			}

			~dirty_opts_node() {
				for (dirty_opts_node* cur_node : this->childs) {
					cur_node->~dirty_opts_node();
					delete cur_node;
				}
			}
		};

		class UnsavedChangesDialog : public wxDialog
		{
		public:
			UnsavedChangesDialog(wxWindow* parent, GUI_App* app, const wxString& header, const wxString& caption = wxMessageBoxCaptionStr, long style = wxOK | wxCENTRE, const wxPoint& pos = wxDefaultPosition);
			~UnsavedChangesDialog();
		private:
			GUI_App* m_app;
			wxScrolledWindow* m_scroller;
			dirty_opts_node* m_dirty_tabs_tree;

			wxWindow* buildScrollWindow(wxString& dirty_tabs);
			void add_dirty_options(Tab* tab, wxWindow* parent, wxBoxSizer* sizer, dirty_opts_node* parent_node);
			void split_dirty_option_by_extruders(const def_opt_pair& pair, std::vector<def_opt_pair>& out);
			std::string getTooltipText(const ConfigOptionDef& def, int index);
			wxBoxSizer* buildYesNoBtns();
			wxBitmap getColourBitmap(const std::string& color);

			dirty_opts_node* buildNode(wxWindow* parent, const wxString& label, dirty_opts_node* parent_node, Tab* tab = NULL, wxSize size = wxDefaultSize);
			template<typename Functor>
			wxCheckBox* buildCheckbox(wxWindow* parent, const wxString& label, const Functor& toggleCallback, wxSize size = wxDefaultSize);
			dirty_opt& buildOption(wxWindow* parent, const wxString& label, dirty_opts_node* parent_node, def_opt_pair val, wxSize size = wxDefaultSize);
		};
	}
}

#endif