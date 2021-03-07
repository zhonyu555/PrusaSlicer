#ifndef slic3r_GUI_Factories_hpp_
#define slic3r_GUI_Factories_hpp_

#include <map>
#include <vector>

#include <wx/bitmap.h>

#include "libslic3r/PrintConfig.hpp"

class wxMenu;
class wxMenuItem;

namespace Slic3r {

enum class ModelVolumeType : int;

namespace GUI {

struct SettingsFactory
{
//				     category ->       vector ( option )
    typedef std::map<std::string, std::vector<std::string>> Bundle;
    static std::map<std::string, std::string>               CATEGORY_ICON;

    static wxBitmap                             get_category_bitmap(const std::string& category_name);
    static Bundle                               get_bundle(const DynamicPrintConfig* config, bool is_object_settings);
    static std::vector<std::string>             get_options(bool is_part);
};

struct MenuFactory
{
    static std::vector<std::pair<std::string, std::string>> ADD_VOLUME_MENU_ITEMS;

    static std::vector<wxBitmap>    get_volume_bitmaps();

    static wxMenu*             append_submenu_add_generic(wxMenu* menu, ModelVolumeType type);
    static void                append_menu_items_add_volume(wxMenu* menu);
    static wxMenuItem*         append_menu_item_split(wxMenu* menu);
    static wxMenuItem*         append_menu_item_layers_editing(wxMenu* menu, wxWindow* parent);
    static wxMenuItem*         append_menu_item_settings(wxMenu* menu);
    static wxMenuItem*         append_menu_item_change_type(wxMenu* menu, wxWindow* parent = nullptr);
    static wxMenuItem*         append_menu_item_instance_to_object(wxMenu* menu, wxWindow* parent);
    static wxMenuItem*         append_menu_item_printable(wxMenu* menu, wxWindow* parent);
    static void                append_menu_items_osx(wxMenu* menu);
    static wxMenuItem*         append_menu_item_fix_through_netfabb(wxMenu* menu);
    static void                append_menu_item_export_stl(wxMenu* menu);
    static void                append_menu_item_reload_from_disk(wxMenu* menu);
    static void                append_menu_item_change_extruder(wxMenu* menu);
    static void                append_menu_item_delete(wxMenu* menu);
    static void                append_menu_item_scale_selection_to_fit_print_volume(wxMenu* menu);
    static void                append_menu_items_convert_unit(wxMenu* menu, int insert_pos = 1); // Add "Conver/Revert..." menu items (from/to inches/meters) after "Reload From Disk"
    static void                append_menu_item_merge_to_multipart_object(wxMenu *menu);
    static void                append_menu_item_merge_to_single_object(wxMenu *menu);

    friend struct SettingsFactory;
};

}}

#endif //slic3r_GUI_Factories_hpp_
