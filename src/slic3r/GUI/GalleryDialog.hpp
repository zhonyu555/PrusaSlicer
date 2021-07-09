#ifndef slic3r_GalleryDialog_hpp_
#define slic3r_GalleryDialog_hpp_

#include "GUI_Utils.hpp"

class wxListCtrl;
class wxImageList;

namespace Slic3r {

namespace GUI {


//------------------------------------------
//          GalleryDialog
//------------------------------------------

class GalleryDialog : public DPIDialog
{
    wxListCtrl*     m_list_ctrl  { nullptr };
    wxImageList*    m_image_list { nullptr };

    struct Item {
        std::string name;
        bool        is_system;
    };
    
    Item    m_selected_item;
    int     ADD_CUSTOM_MODEL_BTN_ID;

    void load_label_icon_list();

    void add_custom_model(wxEvent& event);

public:
    GalleryDialog(wxWindow* parent);
    ~GalleryDialog();

    std::string get_selected_path();

protected:
    void on_dpi_changed(const wxRect& suggested_rect) override;
    void on_sys_color_changed() override {};
};


} // namespace GUI
} // namespace Slic3r

#endif //slic3r_GalleryDialog_hpp_
