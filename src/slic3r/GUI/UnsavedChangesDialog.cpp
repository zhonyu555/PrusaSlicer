#include "UnsavedChangesDialog.hpp"
#include "PresetBundle.hpp"
#include "Tab.hpp"
#include <boost/range/algorithm.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <wx/clrpicker.h>

#define UnsavedChangesDialog_max_width 600
#define UnsavedChangesDialog_max_height 600
#define UnsavedChangesDialog_min_height 200

namespace Slic3r {
	namespace GUI {
		UnsavedChangesDialog::UnsavedChangesDialog(wxWindow* parent, GUI_App* app, const wxString& header, const wxString& caption, long style, const wxPoint& pos)
			: wxDialog(parent, -1, caption, pos, wxSize(UnsavedChangesDialog_max_width, UnsavedChangesDialog_min_height))
		{
			m_app = app;

			SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

			wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

			wxString dirty_tabs;
			wxScrolledWindow* scrolled_win = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE);

				wxBoxSizer* scrolled_sizer = new wxBoxSizer(wxVERTICAL);

				PrinterTechnology printer_technology = app->preset_bundle->printers.get_edited_preset().printer_technology();
				for (Tab* tab : app->tabs_list)
					if (tab->supports_printer_technology(printer_technology) && tab->current_preset_is_dirty()) {
						if (dirty_tabs.empty())
							dirty_tabs = tab->title();
						else
							dirty_tabs += wxString(", ") + tab->title();

						wxStaticText* tabTitle = new wxStaticText(scrolled_win, wxID_ANY, tab->title(), wxDefaultPosition, wxDefaultSize);
						tabTitle->SetFont(GUI::wxGetApp().bold_font());
						scrolled_sizer->Add(tabTitle, 0, wxALL | wxALIGN_LEFT | wxALIGN_TOP, 5);

						add_dirty_options(tab, scrolled_win, scrolled_sizer);
					}
				
				//scrolled_win->SetBackgroundColour(wxColour(255, 0, 0, 100));
				//scrolled_win->ShowScrollbars(wxScrollbarVisibility::wxSHOW_SB_ALWAYS, wxScrollbarVisibility::wxSHOW_SB_ALWAYS);
				scrolled_win->SetSizer(scrolled_sizer);

				scrolled_win->SetScrollRate(0, 2);

			wxStaticText* msg = new wxStaticText(this, wxID_ANY, _(L("The presets on the following tabs were modified")) + ": " + dirty_tabs, wxDefaultPosition, wxDefaultSize);
				wxFont msg_font = GUI::wxGetApp().bold_font();
				msg_font.SetPointSize(10);
				msg->SetFont(msg_font);
				msg->Wrap(UnsavedChangesDialog_max_width - 10);

			wxBoxSizer* cont_sizer = new wxBoxSizer(wxHORIZONTAL);
				wxStaticText* cont_label = new wxStaticText(this, wxID_ANY, _(L("Continue? All unsaved changes will be discarded.")), wxDefaultPosition, wxDefaultSize);
				
				//wxButton* btn_yes = new wxButton(this, wxID_ANY, _(L("Yes")));
				//btn_yes->Bind(wxEVT_BUTTON, ([this](wxCommandEvent) { }));

				//wxButton* btn_no = new wxButton(this, wxID_ANY, _(L("No")));
				//btn_no->Bind(wxEVT_BUTTON, ([this](wxCommandEvent) {}));

			cont_sizer->Add(cont_label, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 5);
			cont_sizer->Add(CreateButtonSizer(wxYES | wxCANCEL | wxNO_DEFAULT));

			main_sizer->Add(msg, 0, wxALL, 5);
			main_sizer->Add(-1, 10);
			main_sizer->Add(scrolled_win, 1, wxEXPAND | wxALL, 5);
			main_sizer->Add(cont_sizer, 0, wxALL | wxALIGN_RIGHT, 5);
			SetSizer(main_sizer);

			this->Layout();
			int x = scrolled_win->GetVirtualSize().y ;
			int y = scrolled_win->GetSize().y;
			int scrolled_add_height = scrolled_win->GetVirtualSize().y - scrolled_win->GetSize().y + 5;

			this->SetSize(wxSize(UnsavedChangesDialog_max_width, std::min(UnsavedChangesDialog_min_height + scrolled_add_height, UnsavedChangesDialog_max_height)));
		}

		void UnsavedChangesDialog::add_dirty_options(Tab* tab, wxWindow* parent, wxBoxSizer* sizer) {
			struct def_opt_pair {
				const ConfigOptionDef* def;
				const ConfigOption* old_opt;
				const ConfigOption* new_opt;
				t_config_option_key key;

				bool operator <(const def_opt_pair& b)
				{
					return this->def->category < b.def->category;
				}
			};
			
			std::vector<def_opt_pair> options;
			
			for (t_config_option_key key : tab->m_presets->current_dirty_options()) {
				def_opt_pair pair;

				pair.def = tab->m_presets->get_selected_preset().config.def()->get(key);
				pair.old_opt = tab->m_presets->get_selected_preset().config.option(key);
				pair.new_opt = tab->m_presets->get_edited_preset().config.option(key);
				pair.key = key;

				options.push_back(pair);
			}

			boost::sort(options);

			std::string lastCat = "";
			int left = 0;
			for (def_opt_pair pair : options) {
				std::string cat = pair.def->category;
				std::string label = pair.def->label;

				const ConfigOption* old_opt = pair.old_opt;
				const ConfigOption* new_opt = pair.new_opt;

				wxStaticText* line;

				if (cat != "") {
					if (cat != lastCat) {
						lastCat = cat;

						sizer->Add(-1, 5);
						line = new wxStaticText(parent, wxID_ANY, cat, wxDefaultPosition, wxDefaultSize);
						line->SetFont(GUI::wxGetApp().bold_font());
						sizer->Add(line, 0, wxLEFT | wxALIGN_LEFT, 15);
					}
					left = 25;
				}
				else {
					left = 0;
				}
				
				
				sizer->Add(-1, 5);

				wxBoxSizer* lineSizer = new wxBoxSizer(wxHORIZONTAL);
					wxStaticText* opt_label = new wxStaticText(parent, wxID_ANY, label, wxDefaultPosition, wxSize(200,-1));
					wxWindow* win_old_opt;
					wxWindow* win_new_opt;

					if(pair.def->gui_type == "color"){
						std::string old_val = old_opt->serialize();
						std::string new_val = new_opt->serialize();

						win_old_opt = new wxStaticText(parent, wxID_ANY, "               ");
						win_old_opt->SetForegroundColour(wxTransparentColor);
						win_old_opt->SetBackgroundColour(wxColour(new_val));

						win_new_opt = new wxColourPickerCtrl(parent, wxID_ANY, wxColour(old_val));
					}
					else {
						switch (pair.def->type) {
							case coFloatOrPercent:
							case coFloat:
							case coFloats:
							case coPercent:
							case coPercents:
							case coString:
							case coStrings:
							case coInt:
							case coInts:
							case coBool:
							case coBools:{
								std::string old_val, new_val;
								
								old_val = old_opt->serialize();
								new_val = new_opt->serialize();

								if (old_opt->is_vector())
									unescape_string_cstyle(old_val, old_val);

								if (new_opt->is_vector())
									unescape_string_cstyle(new_val, new_val);

								if (pair.def->type & coBool) {
									boost::replace_all(old_val, "0", "false");
									boost::replace_all(old_val, "1", "true");
									boost::replace_all(new_val, "0", "false");
									boost::replace_all(new_val, "1", "true");
								}

								win_old_opt = new wxStaticText(parent, wxID_ANY, old_val, wxDefaultPosition, wxDefaultSize);
								win_new_opt = new wxStaticText(parent, wxID_ANY, new_val, wxDefaultPosition, wxDefaultSize);
								break;
							}
							default:
								win_old_opt = new wxStaticText(parent, wxID_ANY, "This control doesn't exist till now", wxDefaultPosition, wxDefaultSize);
								win_new_opt = new wxStaticText(parent, wxID_ANY, "This control doesn't exist till now", wxDefaultPosition, wxDefaultSize);
						}
					}

					win_new_opt->SetForegroundColour(wxGetApp().get_label_clr_modified());

					lineSizer->Add(opt_label);
					wxBoxSizer* old_sizer = new wxBoxSizer(wxVERTICAL);
					old_sizer->Add(win_old_opt, 0, wxALIGN_CENTER_HORIZONTAL);
					wxBoxSizer* new_sizer = new wxBoxSizer(wxVERTICAL);
					new_sizer->Add(win_new_opt, 0, wxALIGN_CENTER_HORIZONTAL);

					lineSizer->Add(old_sizer, 1, wxEXPAND);
					lineSizer->Add(new_sizer, 1, wxEXPAND);

				sizer->Add(lineSizer, 0, wxLEFT | wxALIGN_LEFT | wxEXPAND, left);
			}
		}
	}
}