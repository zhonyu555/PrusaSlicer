#include "UnsavedChangesDialog.hpp"
#include "BitmapCache.hpp"
#include "slic3r/Utils/Diff.hpp"
#include <boost/range/algorithm.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <wx/clrpicker.h>
#include <wx/html/htmlwin.h>

#define Dialog_max_width 1200
#define Dialog_max_height 800

#define Dialog_min_width 600
#define Dialog_min_height 200

#define Dialog_def_border 5
#define Dialog_child_indentation 20

namespace Slic3r {
	namespace GUI {
		UnsavedChangesDialog::UnsavedChangesDialog(wxWindow* parent, GUI_App* app, const wxString& header, const wxString& caption, long style, const wxPoint& pos)
			: wxDialog(parent, -1, caption, pos, wxSize(Dialog_min_width, Dialog_min_height))
		{
			m_app = app;

			SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

			wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

			wxString dirty_tabs;
			buildScrollWindow(dirty_tabs);

			m_msg = new wxStaticText(this, wxID_ANY, _(L("The presets on the following tabs were modified")) + ": " + dirty_tabs, wxDefaultPosition, wxDefaultSize);
				wxFont msg_font = GUI::wxGetApp().normal_font();
				msg_font.SetPointSize(10);
				m_msg->SetFont(msg_font);


			main_sizer->Add(m_msg, 0, wxALL, Dialog_def_border);
			main_sizer->Add(-1, Dialog_def_border);
			main_sizer->Add(m_scroller_container, 1, wxEXPAND | wxALL, Dialog_def_border);
			main_sizer->Add(buildYesNoBtns(), 0, wxEXPAND | wxTOP, Dialog_def_border * 2);
			SetSizer(main_sizer);
			setCorrectSize();
			this->Center();
		}

		UnsavedChangesDialog::~UnsavedChangesDialog() {
			this->m_dirty_tabs_tree->~dirty_opts_node();
			delete this->m_dirty_tabs_tree;
		}

		void UnsavedChangesDialog::setCorrectSize() {
			this->SetSize(wxSize(Dialog_min_width, Dialog_min_height));
			this->Layout();
			int scrolled_add_width = m_scroller->GetVirtualSize().x - m_scroller->GetSize().x + Dialog_def_border;

			int width = std::min(Dialog_min_width + scrolled_add_width, Dialog_max_width);
			m_msg->Wrap(width - Dialog_def_border * 2);

			this->Layout();
			int scrolled_add_height = m_scroller->GetVirtualSize().y - m_scroller->GetSize().y + Dialog_def_border;
			int height = std::min(Dialog_min_height + scrolled_add_height, Dialog_max_height);

			this->SetSize(wxSize(width, height));
		}

		void UnsavedChangesDialog::buildScrollWindow(wxString& dirty_tabs) {
			m_scroller_container = new wxWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE);
			m_scroller_container->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
			wxBoxSizer* wrapper_sizer = new wxBoxSizer(wxVERTICAL);

			buildScroller(dirty_tabs);

			wxStaticLine* line = new wxStaticLine(m_scroller_container, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL);

			wxBoxSizer* btn_sizer = new wxBoxSizer(wxHORIZONTAL);
				m_btn_save = new ScalableButton(m_scroller_container, wxID_ANY, "save", _(L("Save selected")), wxDefaultSize, wxDefaultPosition, wxBU_LEFT | wxBU_EXACTFIT);
				m_btn_save->Bind(wxEVT_BUTTON, &UnsavedChangesDialog::OnBtnSaveSelected, this);

				m_btn_select_all = new wxButton(m_scroller_container, wxID_ANY, _(L("Select All")));
				m_btn_select_all->Bind(wxEVT_BUTTON, [this](wxCommandEvent& e) {
					for (dirty_opts_node* cur_node : m_dirty_tabs_tree->childs) {
						cur_node->checkbox->SetValue(true);
						cur_node->selectChilds(true);
						updateSaveBtn();
					}
				});

				m_btn_select_none = new wxButton(m_scroller_container, wxID_ANY, _(L("Select None")));
				m_btn_select_none->Bind(wxEVT_BUTTON, [this](wxCommandEvent& e) {
					for (dirty_opts_node* cur_node : m_dirty_tabs_tree->childs){
						cur_node->checkbox->SetValue(false);
						cur_node->selectChilds(false);
						updateSaveBtn();
					}
				});

			btn_sizer->Add(m_btn_save);
			btn_sizer->Add(m_btn_select_all, 0, wxLEFT | wxRIGHT, Dialog_def_border);
			btn_sizer->Add(m_btn_select_none);

			wrapper_sizer->Add(m_scroller, 1, wxEXPAND);
			wrapper_sizer->AddSpacer(Dialog_def_border * 2);
			wrapper_sizer->Add(line, 0, wxLEFT | wxRIGHT | wxEXPAND, Dialog_def_border);
			wrapper_sizer->Add(btn_sizer, 0, wxALL, Dialog_def_border);

			m_scroller_container->SetSizer(wrapper_sizer);
		}

		void UnsavedChangesDialog::buildScroller(wxString& dirty_tabs) {
			if (m_dirty_tabs_tree != NULL) {
				m_dirty_tabs_tree->~dirty_opts_node();
				delete m_dirty_tabs_tree;
			}
			m_dirty_tabs_tree = new dirty_opts_node();

			if (m_scroller == NULL) {
				m_scroller = new wxScrolledWindow(m_scroller_container, wxID_ANY);
			}
			else {
				m_scroller->DestroyChildren();
			}

			wxBoxSizer* scrolled_sizer = new wxBoxSizer(wxVERTICAL);

			PrinterTechnology printer_technology = m_app->preset_bundle->printers.get_edited_preset().printer_technology();
			bool highlight = false;
			int itemCount = 0;
			for (Tab* cur_tab : m_app->tabs_list) {
				if (cur_tab->supports_printer_technology(printer_technology) && cur_tab->current_preset_is_dirty()) {
					itemCount++;

					if (dirty_tabs.empty())
						dirty_tabs = cur_tab->title();
					else
						dirty_tabs += wxString(", ") + cur_tab->title();

					wxPanel* cur_tab_win = new wxPanel(m_scroller, wxID_ANY);
					wxBoxSizer* cur_tab_sizer = new wxBoxSizer(wxVERTICAL);
					dirty_opts_node* cur_tab_node = buildNode(cur_tab_win, cur_tab->title(), this->m_dirty_tabs_tree, cur_tab);
					wxCheckBox* cur_tab_cb = cur_tab_node->checkbox;
					cur_tab_cb->SetFont(GUI::wxGetApp().bold_font());

					wxColour background = highlight ? wxSystemSettings::GetColour(wxSYS_COLOUR_FRAMEBK) : wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
					highlight = !highlight;

					cur_tab_sizer->Add(cur_tab_cb, 0, wxALL | wxALIGN_LEFT | wxALIGN_TOP, Dialog_def_border);
					add_dirty_options(cur_tab, cur_tab_win, cur_tab_sizer, cur_tab_node, background);
					cur_tab_sizer->AddSpacer(Dialog_def_border);

					cur_tab_win->SetSizer(cur_tab_sizer);
					cur_tab_win->SetBackgroundColour(background);

					scrolled_sizer->Add(cur_tab_win, 0, wxEXPAND);
				}
			}

			if (!itemCount) {
				wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

				wxStaticBitmap* icon = new wxStaticBitmap(m_scroller, wxID_ANY, create_scaled_bitmap(nullptr, "tick_mark", 24));
				wxStaticText* msg = new wxStaticText(m_scroller, wxID_ANY, _(L("Successfully saved")));

				sizer->Add(icon, 0, wxTOP | wxLEFT | wxRIGHT, Dialog_def_border);
				sizer->Add(msg, 0, wxALIGN_CENTER_VERTICAL);

				scrolled_sizer->AddSpacer(Dialog_def_border * 2);
				scrolled_sizer->Add(sizer);

				m_btn_save->Enable(false);
				m_btn_select_all->Enable(false);
				m_btn_select_none->Enable(false);

				dirty_tabs = "-";
			}

			m_scroller->SetSizer(scrolled_sizer, true);
			m_scroller->SetScrollRate(2, 2);
		}

		void UnsavedChangesDialog::add_dirty_options(Tab* tab, wxWindow* parent, wxBoxSizer* sizer, dirty_opts_node* parent_node, wxColour bg_colour) {
			std::vector<dirty_opt> options;

			for (t_config_option_key key : tab->m_presets->current_dirty_options()) {
				dirty_opt opt(tab->m_presets->get_selected_preset().config.def()->get(key),
					tab->m_presets->get_selected_preset().config.option(key),
					tab->m_presets->get_edited_preset().config.option(key),
					key);

				if (opt.def->category.find('#') != std::string::npos && opt.old_opt->type() & opt.new_opt->type() & coVectorType) {
					split_dirty_option_by_extruders(opt, options);
				}
				else {
					options.push_back(opt);
				}
			}

			if (tab->type() == Slic3r::Preset::TYPE_PRINTER) {
				size_t old_num_extr = tab->m_presets->get_selected_preset().get_num_extruders();
				size_t new_num_extr = tab->m_presets->get_edited_preset().get_num_extruders();

				if (new_num_extr > old_num_extr) {
					for (int i = old_num_extr + 1; i <= new_num_extr; i++) {
						dirty_opt opt(dirty_opt::Type::ExtruderWasAdded, i);
						options.push_back(opt);
					}
				}
				else if (new_num_extr < old_num_extr) {
					for (int i = new_num_extr + 1; i <= old_num_extr; i++) {
						dirty_opt opt(dirty_opt::Type::ExtruderWasRemoved, i);
						options.push_back(opt);
					}
				}
			}

			boost::sort(options);

			std::string lastCat = "";
			dirty_opts_node* category_node;

			for (dirty_opt cur_opt : options) {
				/* Figure out Category */
				std::string cat = cur_opt.dialog_category;

				if (cur_opt.extrIdx >= 0) {
					size_t tag_pos = cat.find("#");
					if (tag_pos != std::string::npos)
					{
						cat.replace(tag_pos, 1, std::to_string(cur_opt.extrIdx + 1));
					}
				}

				if (cat == "") {
					cat = "Other";
				}

				if (cat != lastCat) {
					lastCat = cat;

					sizer->Add(-1, Dialog_def_border);
					category_node = buildNode(parent, cat, parent_node);
					wxCheckBox* cat_cb = category_node->checkbox;
					cat_cb->SetValue(true);
					cat_cb->SetFont(GUI::wxGetApp().bold_font());
					sizer->Add(cat_cb, 0, wxLEFT | wxALIGN_LEFT, Dialog_def_border + Dialog_child_indentation);
				}
				/* End Category */
				
				sizer->Add(-1, Dialog_def_border);

				wxBoxSizer* lineSizer = new wxBoxSizer(wxHORIZONTAL);
					dirty_opt_entry& cur_opt_entry = buildOptionEntry(parent, category_node, cur_opt, wxSize(200,-1));
					wxCheckBox* opt_label = cur_opt_entry.checkbox;
					lineSizer->Add(opt_label);

					if (cur_opt.type == dirty_opt::Type::ConfigOption) {
						buildWindowsForOpt(cur_opt_entry, parent, bg_colour);

						wxBoxSizer* old_sizer = new wxBoxSizer(wxVERTICAL);
						old_sizer->Add(cur_opt_entry.old_win, 0, wxALIGN_CENTER_HORIZONTAL);

						wxBoxSizer* new_sizer = new wxBoxSizer(wxVERTICAL);
						new_sizer->Add(cur_opt_entry.new_win, 0, wxALIGN_CENTER_HORIZONTAL);

						lineSizer->Add(old_sizer, 1, wxEXPAND);
						lineSizer->AddSpacer(30);
						lineSizer->Add(new_sizer, 1, wxEXPAND);
					}

				sizer->Add(lineSizer, 0, wxLEFT | wxALIGN_LEFT | wxEXPAND, Dialog_def_border + Dialog_child_indentation * 2);
			}
		}

		wxBitmap UnsavedChangesDialog::getColourBitmap(const std::string& color) {
			const double em = Slic3r::GUI::wxGetApp().em_unit();
			const int icon_width = lround(6.4 * em);
			const int icon_height = lround(1.6 * em);

			unsigned char rgb[3];
			if (!Slic3r::PresetBundle::parse_color(color, rgb)) {
				wxBitmap bmp = BitmapCache::mksolid(icon_width, icon_height, 0, 0, 0, wxALPHA_TRANSPARENT);

				wxMemoryDC dc(bmp);
				if (!dc.IsOk()) 
					return bmp;

				dc.SetTextForeground(*wxWHITE);
				dc.SetFont(wxGetApp().normal_font());

				const wxRect rect = wxRect(0, 0, bmp.GetWidth(), bmp.GetHeight());
				dc.DrawLabel("undef", rect, wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL);

				dc.SelectObject(wxNullBitmap);

				return bmp;
			}
			else {
				wxBitmap bmp = BitmapCache::mksolid(icon_width, icon_height, rgb);
				return bmp;
			}
		}

		void UnsavedChangesDialog::split_dirty_option_by_extruders(const dirty_opt& opt, std::vector<dirty_opt>& out) {
			std::vector<std::string> old_vals;
			std::vector<std::string> new_vals;

			switch (opt.def->type)
			{
				case coFloats:
					old_vals = ((ConfigOptionFloats*)opt.old_opt)->vserialize();
					new_vals = ((ConfigOptionFloats*)opt.new_opt)->vserialize();
					break;
				case coInts:
					old_vals = ((ConfigOptionInts*)opt.old_opt)->vserialize();
					new_vals = ((ConfigOptionInts*)opt.new_opt)->vserialize();
					break;
				case coStrings:
					old_vals = ((ConfigOptionStrings*)opt.old_opt)->vserialize();
					new_vals = ((ConfigOptionStrings*)opt.new_opt)->vserialize();
					break;
				case coPercents:
					old_vals = ((ConfigOptionPercents*)opt.old_opt)->vserialize();
					new_vals = ((ConfigOptionPercents*)opt.new_opt)->vserialize();
					break;
				case coPoints:
					old_vals = ((ConfigOptionPoints*)opt.old_opt)->vserialize();
					new_vals = ((ConfigOptionPoints*)opt.new_opt)->vserialize();
					break;
				case coBools:
					old_vals = ((ConfigOptionBools*)opt.old_opt)->v_to_string();
					new_vals = ((ConfigOptionBools*)opt.new_opt)->v_to_string();
					break;
				default:
					return;
			}

			for (size_t i = 0; i < old_vals.size() && i < new_vals.size(); i++) {
				std::string cur_old_val = old_vals[i];
				std::string cur_new_val = new_vals[i];

				if (cur_old_val != cur_new_val) {
					dirty_opt new_opt(opt);
					new_opt.extrIdx = i;
					new_opt.ser_old_opt = cur_old_val;
					new_opt.ser_new_opt = cur_new_val;

					out.push_back(new_opt);
				}
			}
		}

		void UnsavedChangesDialog::buildWindowsForOpt(dirty_opt_entry& opt, wxWindow* parent, wxColour bg_colour){
			const ConfigOption* old_opt = opt.val.old_opt;
			const ConfigOption* new_opt = opt.val.new_opt;

			wxWindow* win_old_opt;
			wxWindow* win_new_opt;

			std::string old_val;
			std::string new_val;

			dirty_opt val = opt.val;

			if (val.extrIdx >= 0) {
				old_val = val.ser_old_opt;
				new_val = val.ser_new_opt;
			}
			else {
				old_val = old_opt->to_string();
				new_val = new_opt->to_string();
			}

			if (val.def->gui_type == "color") {
				win_old_opt = old_val == "-" ?
					(wxWindow*)new wxStaticText(parent, wxID_ANY, "-", wxDefaultPosition, wxDefaultSize)
					: (wxWindow*)new wxStaticBitmap(parent, wxID_ANY, getColourBitmap(old_val));

				win_new_opt = new_val == "-" ?
					(wxWindow*)new wxStaticText(parent, wxID_ANY, "-", wxDefaultPosition, wxDefaultSize)
					: (wxWindow*)new wxStaticBitmap(parent, wxID_ANY, getColourBitmap(new_val));
			}
			else {
				switch (val.def->type) {
				case coString:
				case coStrings: {
					wxString sText;
					std::string html = std::string(
						"<html>"
						"<body bgcolor=" + wxString::Format(wxT("#%02X%02X%02X"), bg_colour.Red(), bg_colour.Green(), bg_colour.Blue()) + ">");

					using namespace slic3r;
					Diff diff = Diff(old_val, new_val);

					auto fakeAlpha = [](wxColour front, wxColour back, unsigned char alpha) -> wxString {
						unsigned char r, g, b;
						r = front.Red() * alpha / 255 + back.Red() * (255 - alpha) / 255;
						g = front.Green() * alpha / 255 + back.Green() * (255 - alpha) / 255;
						b = front.Blue() * alpha / 255 + back.Blue() * (255 - alpha) / 255;

						return wxString::Format(wxT("#%02X%02X%02X"), r, g, b);
					};

					for (EditScriptAction cur_action : diff.getSolution()) {
						std::string sub, font;
						switch (cur_action.action)
						{
						case EditScriptAction::ActionType::keep: {
							sub = old_val.substr(cur_action.offset, cur_action.count);
							break;
						}
						case EditScriptAction::ActionType::remove: {
							sub = old_val.substr(cur_action.offset, cur_action.count);
							font = "<font bgcolor=" + fakeAlpha(wxColour(255, 0, 0, 255), bg_colour, 130) + ">";
							break;
						}
						case EditScriptAction::ActionType::insert: {
							sub = new_val.substr(cur_action.offset, cur_action.count);
							font = "<font bgcolor=" + fakeAlpha(wxColour(0, 255, 0, 255), bg_colour, 130) + ">";
							break;
						}
						}

						if (font != "") {
							html += font + sub + "</font>";
							sText += sub;
						}
						else {
							html += sub;
							sText += sub;
						}
					}

					html += wxString(
						"</body>"
						"</html>");

					boost::replace_all(html, "\n", "<br />");

					//wxHtmlWindow seems to need a fixed size, so we find out what size a staticText with the same content would have and use that.
					wxStaticText* win_test = new wxStaticText(parent, wxID_ANY, sText, wxDefaultPosition, wxDefaultSize);
					wxSize size = win_test->GetSize();
					win_test->Destroy();
					size.x += 5;

					wxFont font = GetFont();
					const int fs = font.GetPointSize();
					int fSize[] = { fs,fs,fs,fs,fs,fs,fs };

					wxHtmlWindow* html_win = new wxHtmlWindow(parent, wxID_ANY, wxDefaultPosition, size, wxHW_SCROLLBAR_NEVER);
					html_win->SetBorders(0);
					html_win->SetFonts(font.GetFaceName(), font.GetFaceName(), fSize);
					html_win->SetPage(html);
					html_win->Layout();
					html_win->Refresh();

					win_new_opt = new wxStaticText(parent, wxID_ANY, new_val, wxDefaultPosition, wxDefaultSize);
					win_old_opt = (wxWindow*)html_win;

					break;
				}
				case coFloatOrPercent:
				case coFloat:
				case coFloats:
				case coPercent:
				case coPercents:
				case coInt:
				case coInts:
				case coBool:
				case coBools:
				case coEnum:
				case coPoint:
				case coPoint3:
				case coPoints: {
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

			std::string tooltip = getTooltipText(*opt.val.def, opt.val.extrIdx);
			win_new_opt->SetToolTip(tooltip);
			win_old_opt->SetToolTip(tooltip);

			opt.old_win = win_old_opt;
			opt.new_win = win_new_opt;
		}

		std::string UnsavedChangesDialog::getTooltipText(const ConfigOptionDef &def, int extrIdx) {
			wxString default_val = def.default_value->to_string();

			std::string opt_id = def.opt_key;
			if (extrIdx >= 0) {
				opt_id += "[" + std::to_string(extrIdx) + "]";
			}

			std::string tooltip = def.tooltip;
			if (tooltip.length() > 0)
				tooltip = tooltip + "\n" + _(L("default value")) + "\t: " +
				(boost::iends_with(opt_id, "_gcode") ? "\n" : "") + default_val +
				(boost::iends_with(opt_id, "_gcode") ? "" : "\n") +
				_(L("parameter name")) + "\t: " + opt_id;

			return tooltip;
		}

		void UnsavedChangesDialog::updateSaveBtn() {
			if (m_dirty_tabs_tree->hasAnythingToSave()) {
				m_btn_save->Enable();
			}
			else {
				m_btn_save->Enable(false);
			}
		}

		wxBoxSizer* UnsavedChangesDialog::buildYesNoBtns() {
			wxBoxSizer* cont_stretch_sizer = new wxBoxSizer(wxHORIZONTAL);
			wxPanel* cont_win = new wxPanel(this, wxID_ANY);
			cont_win->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_FRAMEBK));
			wxBoxSizer* cont_sizer = new wxBoxSizer(wxHORIZONTAL);
			wxStaticText* cont_label = new wxStaticText(cont_win, wxID_ANY, _(L("Continue? All unsaved changes will be discarded.")), wxDefaultPosition, wxDefaultSize);
			wxButton* btn_yes = new wxButton(cont_win, wxID_ANY, "Yes");
			btn_yes->Bind(wxEVT_BUTTON, ([this](wxCommandEvent& e) {
				EndModal(wxID_YES);
			}));

			wxButton* btn_no = new wxButton(cont_win, wxID_ANY, "No");
			btn_no->Bind(wxEVT_BUTTON, ([this](wxCommandEvent& e) {
				EndModal(wxID_NO);
			}));
			btn_no->SetFocus();

			cont_sizer->AddStretchSpacer();
			cont_sizer->Add(cont_label, 0, wxALL | wxALIGN_CENTER_VERTICAL, Dialog_def_border * 3);

			cont_sizer->Add(btn_yes, 0, wxALIGN_CENTER_VERTICAL);
			cont_sizer->Add(btn_no, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, Dialog_def_border);

			cont_win->SetSizer(cont_sizer);
			cont_stretch_sizer->Add(cont_win, 1);

			return cont_stretch_sizer;
		}

		dirty_opts_node* UnsavedChangesDialog::buildNode(wxWindow* parent, const wxString& label, dirty_opts_node* parent_node, Tab* tab, wxSize size) {
			dirty_opts_node* node = new dirty_opts_node();

			node->label = label;
			node->tab = tab;
			node->checkbox = buildCheckbox(parent, label, [this, node](wxCommandEvent& e) {
				node->enableChilds(node->checkbox->GetValue());
				updateSaveBtn();
			});

			parent_node->childs.push_back(node);
			return node;
		}

		template<typename Functor>
		wxCheckBox* UnsavedChangesDialog::buildCheckbox(wxWindow* parent, const wxString& label, const Functor& toggleCallback, wxSize size) {
			wxCheckBox* cb = new wxCheckBox(parent, wxID_ANY, label, wxDefaultPosition, size);
			cb->SetValue(true);
			cb->Bind(wxEVT_CHECKBOX, toggleCallback);

			return cb;
		}

		dirty_opt_entry& UnsavedChangesDialog::buildOptionEntry(wxWindow* parent, dirty_opts_node* parent_node, dirty_opt opt, wxSize size) {
			wxString label;
			if (opt.type == dirty_opt::Type::ConfigOption) {
				label = opt.def->label;
			}
			else if(opt.type == dirty_opt::Type::ExtruderWasAdded) {
				label = wxString::Format(_(L("Extruder %d added")), opt.extrIdx);
			}
			else if (opt.type == dirty_opt::Type::ExtruderWasRemoved) {
				label = wxString::Format(_(L("Extruder %d removed")), opt.extrIdx);
			}

			wxCheckBox* cb = buildCheckbox(parent, label, [this](wxCommandEvent& e) {updateSaveBtn(); }, size);

			parent_node->opts.push_back(dirty_opt_entry(opt, cb, parent_node));

			return parent_node->opts.back();
		}

		void UnsavedChangesDialog::OnBtnSaveSelected(wxCommandEvent& e)
		{
			SavePresetWindow dlg(this);

			for (dirty_opts_node* cur_tab : m_dirty_tabs_tree->childs) {
				if (!cur_tab->hasAnythingToSave()) {
					continue;
				}

				PresetCollection* m_presets = cur_tab->tab->m_presets;

				const Preset& preset = m_presets->get_selected_preset();
				auto default_name = preset.is_default ? "Untitled" :
					preset.is_system ? (boost::format(_utf8(L("%1% - Copy"))) % preset.name).str() :
					preset.name;

				bool have_extention = boost::iends_with(default_name, ".ini");
				if (have_extention) {
					size_t len = default_name.length() - 4;
					default_name.resize(len);
				}

				std::vector<std::string> values;
				for (size_t i = 0; i < m_presets->size(); ++i) {
					const Preset& preset = m_presets->preset(i);
					if (preset.is_default || preset.is_system || preset.is_external)
						continue;
					values.push_back(preset.name);
				}

				dlg.build_entry(cur_tab->tab->title(), default_name, values, m_presets, cur_tab->tab);
			}

			if (dlg.ShowModal() == wxID_OK){
				for (std::pair<Tab*, std::string> cur_pair : dlg.get_tab_name_pairs()) {
					Tab* cur_tab = cur_pair.first;
					std::string chosen_name = cur_pair.second;

					DynamicPrintConfig& edited_conf = cur_tab->m_presets->get_edited_preset().config;
					DynamicPrintConfig edited_backup(edited_conf);

					std::vector<dirty_opt_entry*> all_opts;

					dirty_opts_node* cur_tab_node = m_dirty_tabs_tree->getTabNode(cur_tab);
					cur_tab_node->getAllOptionEntries(all_opts);

					//this loop sets all options (in the edited preset) - that the user does not want to save - to their old values (from selected preset)
					for (dirty_opt_entry* cur_opt_to_save : all_opts) {
						if (!cur_opt_to_save->saveMe()) {
							int idx = cur_opt_to_save->val.extrIdx;

							if (cur_opt_to_save->val.type == dirty_opt::Type::ConfigOption) {
								if (idx >= 0) {
									ConfigOption* old_opt = cur_opt_to_save->val.old_opt;
									ConfigOption* new_opt = cur_opt_to_save->val.new_opt;

									switch (cur_opt_to_save->val.def->type) {
									case coFloats:
										((ConfigOptionFloats*)new_opt)->set_at(((ConfigOptionFloats*)old_opt), idx, idx);
										break;
									case coInts:
										((ConfigOptionInts*)new_opt)->set_at(((ConfigOptionFloats*)old_opt), idx, idx);
										break;
									case coStrings:
										((ConfigOptionStrings*)new_opt)->set_at(((ConfigOptionFloats*)old_opt), idx, idx);
										break;
									case coPercents:
										((ConfigOptionPercents*)new_opt)->set_at(((ConfigOptionFloats*)old_opt), idx, idx);
										break;
									case coPoints:
										((ConfigOptionPoints*)new_opt)->set_at(((ConfigOptionFloats*)old_opt), idx, idx);
										break;
									case coBools:
										((ConfigOptionBools*)new_opt)->set_at(((ConfigOptionFloats*)old_opt), idx, idx);
										break;
									}
								}
								else {
									ConfigOption* new_opt = edited_conf.option(cur_opt_to_save->val.key);
									new_opt->set(cur_opt_to_save->val.old_opt);
								}
							}
							else if(cur_opt_to_save->val.type == dirty_opt::Type::ExtruderWasAdded){

							}
							else if (cur_opt_to_save->val.type == dirty_opt::Type::ExtruderWasRemoved) {

							}
						}
					}

					//edited conf will become selected conf and be saved to disk
					cur_tab->m_presets->save_current_preset(chosen_name);
					edited_conf = DynamicPrintConfig(std::move(edited_backup));
					cur_tab->update_after_preset_save();
				}

				//refresh the ui
				wxString dirty_tabs;
				buildScroller(dirty_tabs);
				m_msg->SetLabel(_(L("The presets on the following tabs were modified")) + ": " + dirty_tabs);
				setCorrectSize();
			}
		}
	}
}