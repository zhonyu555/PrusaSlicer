#ifndef slic3r_GUI_UnsavedChangesDialog_hpp_
#define slic3r_GUI_UnsavedChangesDialog_hpp_

#include <wx/wx.h>
#include <wx/checkbox.h>
#include <wx/statline.h>
#include "GUI_App.hpp"
#include "Tab.hpp"
#include "PresetBundle.hpp"
#include "libslic3r/Config.hpp"

namespace Slic3r {
	namespace GUI {
		struct def_opt_pair {
			const ConfigOptionDef* def = NULL;
			ConfigOption* old_opt = NULL;
			ConfigOption* new_opt = NULL;
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
			bool isEnabled = true;

			dirty_opts_node* parent;

			dirty_opt(def_opt_pair val, wxCheckBox* checkbox, dirty_opts_node* parent) {
				this->val = val;
				this->checkbox = checkbox;
				this->parent = parent;
			}

			void setEnabled(bool enabled = true) {
				this->checkbox->Enable(enabled);
				this->old_win->Enable(enabled);
				this->new_win->Enable(enabled);
				this->isEnabled = enabled;
			}

			bool saveMe() {
				return this->isEnabled && this->checkbox->GetValue();
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

					if (cur_node->checkbox->GetValue()) {
						cur_node->enableChilds(enabled);
					}
				}

				for (dirty_opt& cur_opt : this->opts) {
					cur_opt.setEnabled(enabled);
				}
			}

			void selectChilds(bool selected = true) {
				for (dirty_opts_node* cur_node : this->childs) {
					cur_node->checkbox->SetValue(selected);
					cur_node->selectChilds(selected);
				}

				for (dirty_opt& cur_opt : this->opts) {
					cur_opt.checkbox->SetValue(selected);
				}

				this->enableChilds(selected);
			}

			void getAllOptions(std::vector<dirty_opt*>& _opts) {
				for (dirty_opts_node* cur_node : this->childs) {
					cur_node->getAllOptions(_opts);
				}

				for (dirty_opt& cur_opt : this->opts) {
					_opts.push_back(&cur_opt);
				}
			}

			bool hasAnythingToSave() {
				for (dirty_opts_node* cur_node : this->childs) {
					if (cur_node->hasAnythingToSave()) {
						return true;
					}
				}

				for (dirty_opt& cur_opt : this->opts) {
					if (cur_opt.saveMe()) {
						return true;
					}
				}

				return false;
			}

			dirty_opts_node* getTabNode(Tab* tab) {
				for (dirty_opts_node* cur_node : this->childs) {
					if (cur_node->tab == tab) {
						return cur_node;
					}
				}

				return NULL;
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
			wxStaticText* m_msg;
			wxScrolledWindow* m_scroller;
			dirty_opts_node* m_dirty_tabs_tree;

			void setCorrectSize();
			wxWindow* buildScrollWindow(wxString& dirty_tabs);
			void add_dirty_options(Tab* tab, wxWindow* parent, wxBoxSizer* sizer, dirty_opts_node* parent_node, wxColour bg_colour);
			void split_dirty_option_by_extruders(const def_opt_pair& pair, std::vector<def_opt_pair>& out);
			std::string getTooltipText(const ConfigOptionDef& def, int index);
			wxBoxSizer* buildYesNoBtns();
			wxBitmap getColourBitmap(const std::string& color);

			dirty_opts_node* buildNode(wxWindow* parent, const wxString& label, dirty_opts_node* parent_node, Tab* tab = NULL, wxSize size = wxDefaultSize);
			template<typename Functor>
			wxCheckBox* buildCheckbox(wxWindow* parent, const wxString& label, const Functor& toggleCallback, wxSize size = wxDefaultSize);
			dirty_opt& buildOption(wxWindow* parent, const wxString& label, dirty_opts_node* parent_node, def_opt_pair val, wxSize size = wxDefaultSize);

			void OnBtnSaveSelected(wxCommandEvent& e);
		};
	}
}

#endif