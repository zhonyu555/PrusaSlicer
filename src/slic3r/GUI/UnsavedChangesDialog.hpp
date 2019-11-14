#ifndef slic3r_GUI_UnsavedChangesDialog_hpp_
#define slic3r_GUI_UnsavedChangesDialog_hpp_

#include <wx/wx.h>
#include <wx/checkbox.h>
#include <wx/statline.h>
#include <wx/html/htmlwin.h>
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
				ExtruderCount
			};

			Type type = Type::Nil;

			std::string page_name = "";
			std::string optgroup_name = "";

			const ConfigOptionDef* def = nullptr;
			ConfigOption* old_opt = nullptr;
			ConfigOption* new_opt = nullptr;
			t_config_option_key key;

			int extruder_index = -1;
			std::string ser_old_opt;
			std::string ser_new_opt;

			//Type::ExtruderCount
			size_t extruders_count_old = 0;
			size_t extruders_count_new = 0;

			dirty_opt(){}

			dirty_opt(const ConfigOptionDef* _def, Type _type) :
				def(_def),
				type(_type),
				key(_def->opt_key),
				page_name(_def->category)
			{}

			dirty_opt(const ConfigOptionDef* def, ConfigOption* _old_opt, ConfigOption* _new_opt, t_config_option_key _key) :
				dirty_opt(def, Type::ConfigOption)
			{
				this->old_opt = _old_opt;
				this->new_opt = _new_opt;
				this->key = _key;
			}

			dirty_opt(const ConfigOptionDef* def, size_t _extruders_count_old, size_t _extruders_count_new) :
				dirty_opt(def, Type::ExtruderCount)
			{
				this->extruders_count_old = _extruders_count_old;
				this->extruders_count_new = _extruders_count_new;
			}

			bool operator <(const dirty_opt& b)
			{
				if (this->page_name != b.page_name) {
					return this->page_name < b.page_name;
				}
				else if (this->optgroup_name != b.optgroup_name) {
					if (this->optgroup_name.empty()) {
						return false;
					}
					if (b.optgroup_name.empty()) {
						return true;
					}

					return this->optgroup_name < b.optgroup_name;
				}
				else if(this->def != nullptr && b.def != nullptr){
					return this->def->label < b.def->label;
				}

				return false;
			}
		};

		struct dirty_opts_node;

		//this binds the gui line of an option and its definition and internal value together. It's an endpoint of the tree and can't have children.
		struct dirty_opt_entry {
			enum class Gui_Type {
				Nil,
				Text,
				Color,
				Html
			};

			dirty_opt val;

			wxCheckBox* checkbox = nullptr;

			wxWindow* old_win = nullptr;
			wxWindow* new_win = nullptr;

			Gui_Type old_win_type = Gui_Type::Nil;
			Gui_Type new_win_type = Gui_Type::Nil;

			typedef std::map<std::string, void*> Aux_Data;
			Aux_Data aux_data;

			bool isEnabled = true;
			dirty_opts_node* parent;

			dirty_opt_entry(dirty_opt val, wxCheckBox* checkbox, dirty_opts_node* parent) {
				this->val = val;
				this->checkbox = checkbox;
				this->parent = parent;
			}

			~dirty_opt_entry() {
				Aux_Data::iterator it;
				for (it = this->aux_data.begin(); it != aux_data.end(); it++) {
					delete it->second;
				}
			}

			void setWinEnabled(wxWindow* win, Gui_Type _gui_type, bool enabled = true) {
				if (win != nullptr) {
					if (_gui_type == Gui_Type::Html) {
						std::string* html = static_cast<std::string*>(enabled ? this->aux_data["html"] : this->aux_data["html_disabled"]);

						dynamic_cast<wxHtmlWindow*>(win)->SetPage(*html);
					}
					else {
						win->Enable(enabled);
					}
				}
			}

			void setValWinsEnabled(bool enabled = true) {
				setWinEnabled(this->old_win, this->old_win_type, enabled);
				setWinEnabled(this->new_win, this->new_win_type, enabled);
			}

			void setEnabled(bool enabled = true) {
				this->checkbox->Enable(enabled);

				if (this->checkbox->GetValue()) {
					this->setValWinsEnabled(enabled);
				}
				this->isEnabled = enabled;
			}

			bool saveMe() {
				return this->isEnabled && this->checkbox->GetValue();
			}

			int get_checkbox_width() {
				return this->checkbox->GetEffectiveMinSize().GetWidth();
			}

			dirty_opt::Type type() {
				return this->val.type;
			}
		};

		//a node represents a parent (tab, category, ...) in the scroll window
		struct dirty_opts_node {
			wxString label = "";
			wxCheckBox* checkbox;
			GrayableStaticBitmap* icon = nullptr;
			wxStaticText* labelCtrl = nullptr;

			Tab* tab = nullptr;
			std::vector<dirty_opts_node*> childs;
			std::vector<dirty_opt_entry*> opts;

			void enableChilds(bool enabled = true) {
				for (dirty_opts_node* cur_node : this->childs) {
					cur_node->checkbox->Enable(enabled);

					if (cur_node->icon != nullptr) {
						cur_node->icon->Enable(enabled);
					}
					if (cur_node->labelCtrl != nullptr) {
						cur_node->labelCtrl->Enable(enabled);
					}

					if (cur_node->checkbox->GetValue()) {
						cur_node->enableChilds(enabled);
					}
				}

				for (dirty_opt_entry* cur_opt : this->opts) {
					cur_opt->setEnabled(enabled);
				}
			}

			void selectChilds(bool selected = true) {
				for (dirty_opts_node* cur_node : this->childs) {
					cur_node->checkbox->SetValue(selected);
					cur_node->selectChilds(selected);
				}

				for (dirty_opt_entry* cur_opt : this->opts) {
					cur_opt->checkbox->SetValue(selected);
				}

				this->enableChilds(selected);
			}

			void getAllOptionEntries(std::vector<dirty_opt_entry*>& _opts, bool only_opts_to_restore = false, dirty_opt::Type type = dirty_opt::Type::Nil) {
				for (dirty_opts_node* cur_node : this->childs) {
					cur_node->getAllOptionEntries(_opts, only_opts_to_restore, type);
				}

				for (dirty_opt_entry* cur_opt : this->opts) {
					if((!only_opts_to_restore || !cur_opt->saveMe()) && (type == dirty_opt::Type::Nil || cur_opt->val.type == type))
					_opts.push_back(cur_opt);
				}
			}

			bool hasAnythingToSave() {
				for (dirty_opts_node* cur_node : this->childs) {
					if (cur_node->hasAnythingToSave()) {
						return true;
					}
				}

				for (dirty_opt_entry* cur_opt : this->opts) {
					if (cur_opt->saveMe()) {
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

				return nullptr;
			}

			int get_label_width() {
				int w = this->checkbox->GetEffectiveMinSize().GetWidth();

				if (this->icon != nullptr) {
					w += this->icon->GetEffectiveMinSize().GetWidth();
				}
				if (this->labelCtrl != nullptr) {
					w += this->labelCtrl->GetEffectiveMinSize().GetWidth();
				}

				return w;
			}

			int get_max_child_label_width() {
				int w = 0;

				for (dirty_opts_node* cur_node : this->childs) {
					w = std::max(w, cur_node->get_max_child_label_width());
				}

				for (dirty_opt_entry* cur_opt : this->opts) {
					w = std::max(w, cur_opt->get_checkbox_width());
				}

				return w;
			}

			~dirty_opts_node() {
				for (dirty_opts_node* cur_node : this->childs) {
					delete cur_node;
				}

				for (dirty_opt_entry* cur_opt_entry : this->opts) {
					delete cur_opt_entry;
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
			wxScrolledWindow* m_scroller = nullptr;

			ScalableButton* m_btn_save;
			wxButton*		m_btn_select_all;
			wxButton*		m_btn_select_none;

			dirty_opts_node* m_dirty_tabs_tree = nullptr;

			typedef std::map<std::string, wxBitmap> PageIconMap;

			void setCorrectSize();
			void buildScrollWindow(wxString& dirty_tabs);	//builds m_scroller_container
			void buildScroller(wxString& dirty_tabs);		//builds m_scroller
			void add_dirty_options(Tab* tab, wxWindow* parent, wxBoxSizer* sizer, dirty_opts_node* parent_node, wxColour bg_colour);
			void get_dirty_options_for_tab(Tab* tab, std::vector<dirty_opt>& out, PageIconMap& page_icons_out);
			void split_dirty_option_by_extruders(const dirty_opt& pair, std::vector<dirty_opt>& out);
			wxBoxSizer* buildYesNoBtns();
			wxBitmap getColourBitmap(const std::string& color);
			void updateSaveBtn();

			dirty_opts_node* buildNode(wxWindow* parent, const wxString& label, dirty_opts_node* parent_node, Tab* tab = nullptr, wxSize size = wxDefaultSize);
			template<typename Functor>
			wxCheckBox* buildCheckbox(wxWindow* parent, const wxString& label, const Functor& toggleCallback, wxSize size = wxDefaultSize, std::string tooltip = "");
			dirty_opt_entry& buildOptionEntry(wxWindow* parent, dirty_opts_node* parent_node, dirty_opt opt, wxColour bg_colour, wxSize size = wxDefaultSize);
			void buildWindowsForOpt(dirty_opt_entry& opt, wxWindow* parent, wxColour bg_colour);
			std::string getTooltipText(const ConfigOptionDef& def, int extrIdx);

			void OnBtnSaveSelected(wxCommandEvent& e);
		};
	}
}

#endif