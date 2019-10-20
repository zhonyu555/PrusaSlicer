#include "UnsavedChangesDialog.hpp"
#include "PresetBundle.hpp"
#include "Tab.hpp"
#include <boost/range/algorithm.hpp>

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
			msg_font.SetPointSize(12);
			msg->SetFont(msg_font);
			msg->Wrap(UnsavedChangesDialog_max_width - 10);

			wxStaticText* msg2 = new wxStaticText(this, wxID_ANY, _(L("Continue? All unsaved changes will be discarded.")), wxDefaultPosition, wxDefaultSize);
			//msg->SetForegroundColour(wxColor(0, 200, 206));

			main_sizer->Add(msg, 0, wxALL, 5);
			main_sizer->Add(-1, 10);
			main_sizer->Add(scrolled_win, 1, wxEXPAND | wxALL, 5);
			main_sizer->Add(msg2, 0, wxALL, 5);
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

				options.push_back(pair);
			}

			boost::sort(options);

			std::string lastCat = "";
			int left = 0;
			for (def_opt_pair pair : options) {
				std::string cat = pair.def->category;
				std::string label = pair.def->label;
				std::string old_val = pair.old_opt->serialize();
				std::string new_val = pair.new_opt->serialize();

				wxStaticText* line;

				if (cat != "") {
					if (cat != lastCat) {
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
					wxStaticText* opt_old = new wxStaticText(parent, wxID_ANY, old_val, wxDefaultPosition, wxDefaultSize);
					wxStaticText* opt_new = new wxStaticText(parent, wxID_ANY, new_val, wxDefaultPosition, wxDefaultSize);
					opt_new->SetForegroundColour(wxGetApp().get_label_clr_modified());

					lineSizer->Add(opt_label);
					lineSizer->AddStretchSpacer();
					lineSizer->Add(opt_old);
					lineSizer->AddStretchSpacer();
					lineSizer->Add(opt_new, 0, wxRIGHT, 25);

				sizer->Add(lineSizer, 0, wxLEFT | wxALIGN_LEFT | wxEXPAND, left);
			}
		}

	}
}