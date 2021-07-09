#include "GalleryDialog.hpp"

#include <cstddef>
#include <vector>
#include <string>
#include <boost/algorithm/string.hpp>

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/statbox.h>
#include <wx/wupdlock.h>

#include "GUI.hpp"
#include "GUI_App.hpp"
#include "format.hpp"
#include "wxExtensions.hpp"
#include "I18N.hpp"
#include "libslic3r/Utils.hpp"
#include "libslic3r/AppConfig.hpp"
#include <wx/notebook.h>
#include "Notebook.hpp"

namespace Slic3r {
namespace GUI {

#define BORDER_W    10
#define IMG_PX_CNT  64

namespace fs = boost::filesystem;

// Gallery::DropTarget
class GalleryDropTarget : public wxFileDropTarget
{
public:
    GalleryDropTarget(wxPanel* plater)/* : m_plater(plater)*/ { this->SetDefaultAction(wxDragCopy); }

    bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames) override;

//private:
//    Plater* m_plater;
};

bool GalleryDropTarget::OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames)
{
#ifdef WIN32
    // hides the system icon
    this->MSWUpdateDragImageOnLeave();
#endif // WIN32
    return true;

//#if ENABLE_PROJECT_DIRTY_STATE
//    bool res = (m_plater != nullptr) ? m_plater->load_files(filenames) : false;
//    wxGetApp().mainframe->update_title();
//    return res;
//#else
//    return (m_plater != nullptr) ? m_plater->load_files(filenames) : false;
//#endif // ENABLE_PROJECT_DIRTY_STATE
}


GalleryDialog::GalleryDialog(wxWindow* parent) :
    DPIDialog(parent, wxID_ANY, _L("Shapes Gallery"), wxDefaultPosition, wxSize(45 * wxGetApp().em_unit(), -1), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
#ifndef _WIN32
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
#endif
    SetFont(wxGetApp().normal_font());

    wxStaticText* label_top = new wxStaticText(this, wxID_ANY, _L("Select shape from the gallery") + ":");

    m_list_ctrl = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(40 * wxGetApp().em_unit(), 35 * wxGetApp().em_unit()),
                                wxLC_ICON | wxLC_NO_HEADER | wxLC_ALIGN_TOP | wxSIMPLE_BORDER | wxLC_SINGLE_SEL);
    m_list_ctrl->Bind(wxEVT_LIST_ITEM_SELECTED, [this](wxListEvent& event) {
        int idx = event.GetIndex();
        m_selected_item.name = into_u8(m_list_ctrl->GetItemText(idx));
        m_selected_item.is_system = m_list_ctrl->GetItemFont(idx).GetWeight() == wxFONTWEIGHT_BOLD;
    });

    wxStdDialogButtonSizer* buttons = this->CreateStdDialogButtonSizer(wxOK | wxCANCEL);

    ADD_CUSTOM_MODEL_BTN_ID = NewControlId();
    auto add_custom_model_btn = new wxButton(this, ADD_CUSTOM_MODEL_BTN_ID, _L("Add custom shape"));
    buttons->Insert(0, add_custom_model_btn, 0, wxLEFT, 5);
    buttons->InsertStretchSpacer(1, 1);
    this->Bind(wxEVT_BUTTON, &GalleryDialog::add_custom_model, this, ADD_CUSTOM_MODEL_BTN_ID);

    load_label_icon_list();

    wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);

    topSizer->Add(label_top , 0, wxEXPAND | wxLEFT | wxTOP | wxRIGHT, BORDER_W);
    topSizer->Add(m_list_ctrl, 1, wxEXPAND | wxLEFT | wxTOP | wxRIGHT, BORDER_W);
    topSizer->Add(buttons   , 0, wxEXPAND | wxALL, BORDER_W);

    SetSizer(topSizer);
    topSizer->SetSizeHints(this);

    wxGetApp().UpdateDlgDarkUI(this);
    this->CenterOnScreen();
}

GalleryDialog::~GalleryDialog()
{   
}

void GalleryDialog::on_dpi_changed(const wxRect& suggested_rect)
{
    const int& em = em_unit();

    msw_buttons_rescale(this, em, { ADD_CUSTOM_MODEL_BTN_ID, wxID_OK, wxID_CANCEL });

    const wxSize& size = wxSize(45 * em, 35 * em);
    SetMinSize(size);

    Fit();
    Refresh();
}

static void add_border(wxImage& image) 
{
    const wxColour& clr = wxGetApp().get_color_hovered_btn_label();

    auto px_data = (uint8_t*)image.GetData();
    auto a_data = (uint8_t*)image.GetAlpha();

    int width = image.GetWidth();
    int height = image.GetHeight();
    int border_width = 2;

    for (size_t x = 0; x < width; ++x) {
        for (size_t y = 0; y < height; ++y) {
            if (x < border_width || y < border_width ||
                x >= (width - border_width) || y >= (height - border_width)) {
                const size_t idx = (x + y * width);
                const size_t idx_rgb = (x + y * width) * 3;
                px_data[idx_rgb] = clr.Red();
                px_data[idx_rgb + 1] = clr.Green();
                px_data[idx_rgb + 2] = clr.Blue();
                if (a_data)
                    a_data[idx] = 255u;
            }
        }
    }
}

static void add_def_img(wxImageList* img_list, bool is_system) 
{
    wxBitmap bmp = create_scaled_bitmap("cog", nullptr, IMG_PX_CNT, true);    

    if (is_system) {
        wxImage image = bmp.ConvertToImage();
        if (image.IsOk() && image.GetWidth() != 0 && image.GetHeight() != 0) {
            add_border(image);
            bmp = wxBitmap(std::move(image));
        }
    }
    img_list->Add(bmp);
};

static fs::path get_dir(bool sys_dir)
{
    return fs::absolute(fs::path(gallery_dir()) / (sys_dir ? "system" : "custom")).make_preferred();
}

static bool custom_exists() 
{
    return fs::exists(fs::absolute(fs::path(gallery_dir()) / "custom").make_preferred());
}

static std::string get_dir_path(bool sys_dir) 
{
    fs::path dir = get_dir(sys_dir);
#ifdef __WXMSW__
    return dir.string() + "\\";
#else
    return dir.string() + "/";
#endif
}

void GalleryDialog::load_label_icon_list()
{
    // load names from files
    auto add_files_from_gallery = [](std::vector<Item>& items, bool sys_dir, std::string& dir_path)
    {
        fs::path dir = get_dir(sys_dir);
        dir_path = get_dir_path(sys_dir);

        for (auto& dir_entry : fs::directory_iterator(dir))
            if (is_stl_file(dir_entry)) {
                std::string name = dir_entry.path().filename().string();
                // Remove the .ini suffix.
                name.erase(name.size() - 4);
                Item item = Item{ name, sys_dir };
                items.push_back(item);
            }
    };

    std::string m_sys_dir_path, m_cust_dir_path;
    std::vector<Item> list_items;
    add_files_from_gallery(list_items, true, m_sys_dir_path);
    if (custom_exists())
        add_files_from_gallery(list_items, false, m_cust_dir_path);

    // Make an image list containing large icons

    int px_cnt = (int)(em_unit() * IMG_PX_CNT * 0.1f + 0.5f);
    m_image_list = new wxImageList(px_cnt, px_cnt);

    std::string ext = ".png";

    for (const auto& item : list_items) {
        std::string img_name = (item.is_system ? m_sys_dir_path : m_cust_dir_path) + item.name + ext;
        if (!fs::exists(img_name)) {
            add_def_img(m_image_list, item.is_system);
            continue;
        }

        wxImage image;
        if (!image.LoadFile(from_u8(img_name), wxBITMAP_TYPE_PNG) ||
            image.GetWidth() == 0 || image.GetHeight() == 0) {
            add_def_img(m_image_list, item.is_system);
            continue;
        }
        image.Rescale(px_cnt, px_cnt, wxIMAGE_QUALITY_BILINEAR);

        if (item.is_system)
            add_border(image);
        wxBitmap bmp = wxBitmap(std::move(image));
        m_image_list->Add(bmp);
    }

    m_list_ctrl->SetImageList(m_image_list, wxIMAGE_LIST_NORMAL);

    int img_cnt = m_image_list->GetImageCount();
    for (int i = 0; i < img_cnt; i++) {
        m_list_ctrl->InsertItem(i, from_u8(list_items[i].name), i);
        if (list_items[i].is_system)
            m_list_ctrl->SetItemFont(i, m_list_ctrl->GetItemFont(i).Bold());
    }
}


std::string GalleryDialog::get_selected_path()
{
    return get_dir_path(m_selected_item.is_system) + m_selected_item.name + ".stl";
}

void GalleryDialog::add_custom_model(wxEvent& event)
{
    wxArrayString input_files;
    wxFileDialog dialog(this, _L("Choose one or more files (STL):"),
        from_u8(wxGetApp().app_config->get_last_dir()), "",
        file_wildcards(FT_STL), wxFD_OPEN | wxFD_MULTIPLE | wxFD_FILE_MUST_EXIST);

    if (dialog.ShowModal() == wxID_OK)
        dialog.GetPaths(input_files);

    if (input_files.IsEmpty())
        return;

    auto dest_dir = get_dir(false);

    try {
        if (!fs::exists(dest_dir))
            if (!fs::create_directory(dest_dir)) {
                std::cerr << "Unable to create destination directory" << dest_dir.string() << '\n' ;
                return;
            }
    }
    catch (fs::filesystem_error const& e) {
        std::cerr << e.what() << '\n';
        return ;
    }

    // Iterate through the source directory
    for (size_t i = 0; i < input_files.size(); ++i) {
        std::string input_file = input_files.Item(i).ToUTF8().data();

        try {
            fs::path current = fs::path(input_file);
            fs::copy_file(current, dest_dir / current.filename());
        }
        catch (fs::filesystem_error const& e) {
            std::cerr << e.what() << '\n';
        }
    }

    m_image_list->RemoveAll();
    m_list_ctrl->ClearAll();
    load_label_icon_list();
}

}}    // namespace Slic3r::GUI
