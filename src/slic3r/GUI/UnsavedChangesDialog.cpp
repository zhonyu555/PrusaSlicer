#include "UnsavedChangesDialog.hpp"
#include "PresetBundle.hpp"
#include "Tab.hpp"
#include <boost/range/algorithm.hpp>

#define UnsavedChangesDialog_max_width 600

namespace Slic3r {
	namespace GUI {
		UnsavedChangesDialog::UnsavedChangesDialog(wxWindow* parent, GUI_App* app, const wxString& header, const wxString& caption, long style, const wxPoint& pos)
			: wxDialog(parent, -1, caption, pos, wxSize(UnsavedChangesDialog_max_width,-1))
		{
			m_app = app;

			SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

			wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

			wxString dirty_tabs;
			wxScrolledWindow* scrolled_win = new wxScrolledWindow(this, wxID_ANY);

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
				
				scrolled_win->SetBackgroundColour(wxColour(255, 0, 0, 100));
				scrolled_win->ShowScrollbars(wxScrollbarVisibility::wxSHOW_SB_ALWAYS, wxScrollbarVisibility::wxSHOW_SB_ALWAYS);
				scrolled_win->SetSizer(scrolled_sizer);

				//scrolled_sizer->FitInside(scrolled_win);
				//scrolled_win->FitInside();
				scrolled_win->SetScrollRate(0, 2);
				//scrolled_sizer->SetSizeHints(scrolled_win);

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
			//main_sizer->SetSizeHints(this);
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

			for (def_opt_pair pair : options) {
				std::string old_val = pair.old_opt->serialize();
				std::string new_val = pair.new_opt->serialize();

				std::string msg = pair.def->category + "|" + pair.def->label + ": " + old_val + " -> " + new_val;

				wxStaticText* opt = new wxStaticText(parent, wxID_ANY, msg, wxDefaultPosition, wxDefaultSize);
				sizer->Add(-1, 5);
				sizer->Add(opt, 0, wxLEFT | wxALIGN_LEFT, 15);
			}
		}

	}
}