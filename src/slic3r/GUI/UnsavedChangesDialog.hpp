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
		struct dirty_opt {
			enum class Type {
				Nil,
				ConfigOption,
				ExtruderWasAdded,
				ExtruderWasRemoved
			};

			Type type = Type::Nil;
			std::string dialog_category;

			const ConfigOptionDef* def = NULL;
			ConfigOption* old_opt = NULL;
			ConfigOption* new_opt = NULL;
			t_config_option_key key;

			int extrIdx = -1;
			std::string ser_old_opt;
			std::string ser_new_opt;

			dirty_opt(){}

			dirty_opt(const ConfigOptionDef* def, ConfigOption* old_opt, ConfigOption* new_opt, t_config_option_key key) :
				def(def), old_opt(old_opt), new_opt(new_opt), key(key)
			{
				this->type = Type::ConfigOption;
				this->dialog_category = def->category;
			}

			dirty_opt(Type type, int extruder_index)
			{
				this->type = type;
				this->extrIdx = extruder_index;
				this->dialog_category = _(L("Extruders"));
			}


			bool operator <(const dirty_opt& b)
			{
				if (this->type == Type::ConfigOption && this->dialog_category == b.dialog_category && this->extrIdx >= 0 && b.extrIdx >= 0)
				{
					return this->extrIdx < b.extrIdx;
				}

				return this->dialog_category < b.dialog_category;
			}
		};

		struct dirty_opts_node;

		//this binds the gui line of an option and its definition and internal value together
		struct dirty_opt_entry {
			dirty_opt val;

			wxCheckBox* checkbox = NULL;
			wxWindow* old_win = NULL;
			wxWindow* new_win = NULL;

			bool isEnabled = true;
			dirty_opts_node* parent;

			dirty_opt_entry(dirty_opt val, wxCheckBox* checkbox, dirty_opts_node* parent) {
				this->val = val;
				this->checkbox = checkbox;
				this->parent = parent;
			}

			void setEnabled(bool enabled = true) {
				this->checkbox->Enable(enabled);

				if (this->old_win != NULL) {
					this->old_win->Enable(enabled);
				}
				if (this->new_win != NULL) {
					this->new_win->Enable(enabled);
				}

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
			std::vector<dirty_opt_entry> opts;

			void enableChilds(bool enabled = true) {
				for (dirty_opts_node* cur_node : this->childs) {
					cur_node->checkbox->Enable(enabled);

					if (cur_node->checkbox->GetValue()) {
						cur_node->enableChilds(enabled);
					}
				}

				for (dirty_opt_entry& cur_opt : this->opts) {
					cur_opt.setEnabled(enabled);
				}
			}

			void selectChilds(bool selected = true) {
				for (dirty_opts_node* cur_node : this->childs) {
					cur_node->checkbox->SetValue(selected);
					cur_node->selectChilds(selected);
				}

				for (dirty_opt_entry& cur_opt : this->opts) {
					cur_opt.checkbox->SetValue(selected);
				}

				this->enableChilds(selected);
			}

			void getAllOptionEntries(std::vector<dirty_opt_entry*>& _opts) {
				for (dirty_opts_node* cur_node : this->childs) {
					cur_node->getAllOptionEntries(_opts);
				}

				for (dirty_opt_entry& cur_opt : this->opts) {
					_opts.push_back(&cur_opt);
				}
			}

			bool hasAnythingToSave() {
				for (dirty_opts_node* cur_node : this->childs) {
					if (cur_node->hasAnythingToSave()) {
						return true;
					}
				}

				for (dirty_opt_entry& cur_opt : this->opts) {
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
			wxWindow* m_scroller_container;
			wxScrolledWindow* m_scroller = NULL;

			ScalableButton* m_btn_save;
			wxButton*		m_btn_select_all;
			wxButton*		m_btn_select_none;

			dirty_opts_node* m_dirty_tabs_tree = NULL;

			void setCorrectSize();
			void buildScrollWindow(wxString& dirty_tabs);	//builds m_scroller_container
			void buildScroller(wxString& dirty_tabs);		//builds m_scroller
			void add_dirty_options(Tab* tab, wxWindow* parent, wxBoxSizer* sizer, dirty_opts_node* parent_node, wxColour bg_colour);
			void split_dirty_option_by_extruders(const dirty_opt& pair, std::vector<dirty_opt>& out);
			void buildWindowsForOpt(dirty_opt_entry& opt, wxWindow* parent, wxColour bg_colour);
			std::string getTooltipText(const ConfigOptionDef& def, int extrIdx);
			wxBoxSizer* buildYesNoBtns();
			wxBitmap getColourBitmap(const std::string& color);
			void updateSaveBtn();

			dirty_opts_node* buildNode(wxWindow* parent, const wxString& label, dirty_opts_node* parent_node, Tab* tab = NULL, wxSize size = wxDefaultSize);
			template<typename Functor>
			wxCheckBox* buildCheckbox(wxWindow* parent, const wxString& label, const Functor& toggleCallback, wxSize size = wxDefaultSize);
			dirty_opt_entry& buildOptionEntry(wxWindow* parent, dirty_opts_node* parent_node, dirty_opt opt, wxSize size = wxDefaultSize);

			void OnBtnSaveSelected(wxCommandEvent& e);
		};
	}
}

#endif