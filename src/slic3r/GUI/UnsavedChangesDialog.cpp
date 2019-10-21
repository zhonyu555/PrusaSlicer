#include "UnsavedChangesDialog.hpp"
#include "PresetBundle.hpp"
#include "Tab.hpp"
#include "BitmapCache.hpp"
#include <boost/range/algorithm.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <wx/clrpicker.h>

#define UnsavedChangesDialog_max_width 1200
#define UnsavedChangesDialog_max_height 800

#define UnsavedChangesDialog_min_width 600
#define UnsavedChangesDialog_min_height 200

namespace Slic3r {
	namespace GUI {
		UnsavedChangesDialog::UnsavedChangesDialog(wxWindow* parent, GUI_App* app, const wxString& header, const wxString& caption, long style, const wxPoint& pos)
			: wxDialog(parent, -1, caption, pos, wxSize(UnsavedChangesDialog_min_width, UnsavedChangesDialog_min_height))
		{
			m_app = app;

			SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

			wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

			wxString dirty_tabs;
			wxScrolledWindow* scrolled_win = buildScrollWindow(dirty_tabs);

			wxStaticText* msg = new wxStaticText(this, wxID_ANY, _(L("The presets on the following tabs were modified")) + ": " + dirty_tabs, wxDefaultPosition, wxDefaultSize);
				wxFont msg_font = GUI::wxGetApp().bold_font();
				msg_font.SetPointSize(10);
				msg->SetFont(msg_font);
				msg->Wrap(UnsavedChangesDialog_max_width - 10);

			wxWindow* cont_win = new wxWindow(this, wxID_ANY);
				cont_win->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_FRAMEBK));
				wxBoxSizer* cont_sizer = new wxBoxSizer(wxHORIZONTAL);
				wxStaticText* cont_label = new wxStaticText(cont_win, wxID_ANY, _(L("Continue? All unsaved changes will be discarded.")), wxDefaultPosition, wxDefaultSize);
				
				cont_sizer->Add(cont_label, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 5);
				cont_sizer->Add(CreateButtonSizer(wxYES | wxNO | wxNO_DEFAULT));

				cont_win->SetSizer(cont_sizer);

			main_sizer->Add(msg, 0, wxALL, 5);
			main_sizer->Add(-1, 10);
			main_sizer->Add(scrolled_win, 1, wxEXPAND | wxALL, 5);
			main_sizer->Add(cont_win, 1, wxALL | wxALIGN_RIGHT, 5);
			SetSizer(main_sizer);

			this->Layout();
			int scrolled_add_width = scrolled_win->GetVirtualSize().x - scrolled_win->GetSize().x + 5;
			int scrolled_add_height = scrolled_win->GetVirtualSize().y - scrolled_win->GetSize().y + 5;

			this->SetSize(wxSize(std::min(UnsavedChangesDialog_min_width + scrolled_add_width, UnsavedChangesDialog_max_width), std::min(UnsavedChangesDialog_min_height + scrolled_add_height, UnsavedChangesDialog_max_height)));
		}

		wxScrolledWindow* UnsavedChangesDialog::buildScrollWindow(wxString& dirty_tabs) {
			wxScrolledWindow* scrolled_win = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE);

			wxBoxSizer* scrolled_sizer = new wxBoxSizer(wxVERTICAL);

			PrinterTechnology printer_technology = m_app->preset_bundle->printers.get_edited_preset().printer_technology();
			bool highlight = false;
			for (Tab* tab : m_app->tabs_list)
				if (tab->supports_printer_technology(printer_technology) && tab->current_preset_is_dirty()) {
					if (dirty_tabs.empty())
						dirty_tabs = tab->title();
					else
						dirty_tabs += wxString(", ") + tab->title();

					wxWindow* cur_tab_win = new wxWindow(scrolled_win, wxID_ANY);
					wxBoxSizer* cur_tab_sizer = new wxBoxSizer(wxVERTICAL);

					wxStaticText* tabTitle = new wxStaticText(cur_tab_win, wxID_ANY, tab->title(), wxDefaultPosition, wxDefaultSize);
						tabTitle->SetFont(GUI::wxGetApp().bold_font());

					cur_tab_sizer->Add(tabTitle, 0, wxALL | wxALIGN_LEFT | wxALIGN_TOP, 5);
					add_dirty_options(tab, cur_tab_win, cur_tab_sizer);

					cur_tab_win->SetSizer(cur_tab_sizer);

					wxColour background = highlight ? wxSystemSettings::GetColour(wxSYS_COLOUR_FRAMEBK) : wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
					highlight = !highlight;
					cur_tab_win->SetBackgroundColour(background);

					scrolled_sizer->Add(cur_tab_win, 1, wxEXPAND);
				}

			scrolled_win->SetSizer(scrolled_sizer);
			scrolled_win->SetScrollRate(2, 2);

			return scrolled_win;
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

						const double em = Slic3r::GUI::wxGetApp().em_unit();
						const int icon_width = lround(3.2 * em);
						const int icon_height = lround(1.6 * em);

						unsigned char rgb[3];
						Slic3r::PresetBundle::parse_color(old_val, rgb);

						win_old_opt = new wxStaticBitmap(parent, wxID_ANY, BitmapCache::mksolid(icon_width, icon_height, rgb));
						win_new_opt = new wxColourPickerCtrl(parent, wxID_ANY, wxColour(new_val));
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

								if (old_opt->is_vector() || pair.def->type == coString)
									unescape_string_cstyle(old_val, old_val);

								if (new_opt->is_vector() || pair.def->type == coString)
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
					
					std::string tooltip = getTooltipText(*pair.def);
						
					win_new_opt->SetToolTip(tooltip);
					win_old_opt->SetToolTip(tooltip);

					lineSizer->Add(opt_label);

					wxBoxSizer* old_sizer = new wxBoxSizer(wxVERTICAL);
					old_sizer->Add(win_old_opt, 0, wxALIGN_CENTER_HORIZONTAL);

					wxBoxSizer* new_sizer = new wxBoxSizer(wxVERTICAL);
					new_sizer->Add(win_new_opt, 0, wxALIGN_CENTER_HORIZONTAL);

					lineSizer->Add(old_sizer, 1, wxEXPAND);
					lineSizer->AddSpacer(30);
					lineSizer->Add(new_sizer, 1, wxEXPAND);

				sizer->Add(lineSizer, 0, wxLEFT | wxALIGN_LEFT | wxEXPAND, left);
			}
		}

		std::string UnsavedChangesDialog::getTooltipText(const ConfigOptionDef &def) {
			int opt_idx = 0;
			switch (def.type)
			{
			case coPercents:
			case coFloats:
			case coStrings:
			case coBools:
			case coInts: {
				auto tag_pos = def.opt_key.find("#");
				if (tag_pos != std::string::npos)
					opt_idx = stoi(def.opt_key.substr(tag_pos + 1, def.opt_key.size()));
				break;
			}
			default:
				break;
			}

			wxString default_val = wxString("");

			switch (def.type) {
			case coFloatOrPercent:
			{
				default_val = double_to_string(def.default_value->getFloat());
				if (def.get_default_value<ConfigOptionFloatOrPercent>()->percent)
					default_val += "%";
				break;
			}
			case coPercent:
			{
				default_val = wxString::Format(_T("%i"), int(def.default_value->getFloat()));
				default_val += "%";
				break;
			}
			case coPercents:
			case coFloats:
			case coFloat:
			{
				double val = def.type == coFloats ?
					def.get_default_value<ConfigOptionFloats>()->get_at(opt_idx) :
					def.type == coFloat ?
					def.default_value->getFloat() :
					def.get_default_value<ConfigOptionPercents>()->get_at(opt_idx);
				default_val = double_to_string(val);
				break;
			}
			case coString:
				default_val = def.get_default_value<ConfigOptionString>()->value;
				break;
			case coStrings:
			{
				const ConfigOptionStrings* vec = def.get_default_value<ConfigOptionStrings>();
				if (vec == nullptr || vec->empty()) break; //for the case of empty default value
				default_val = vec->get_at(opt_idx);
				break;
			}
			default:
				break;
			}

			std::string opt_id = def.opt_key;
			auto hash_pos = opt_id.find("#");
			if (hash_pos != std::string::npos) {
				opt_id.replace(hash_pos, 1, "[");
				opt_id += "]";
			}

			std::string tooltip = def.tooltip;
			if (tooltip.length() > 0)
				tooltip = tooltip + "\n" + _(L("default value")) + "\t: " +
				(boost::iends_with(opt_id, "_gcode") ? "\n" : "") + default_val +
				(boost::iends_with(opt_id, "_gcode") ? "" : "\n") +
				_(L("parameter name")) + "\t: " + opt_id;

			return tooltip;
		}
	}
}