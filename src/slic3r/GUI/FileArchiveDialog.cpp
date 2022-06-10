#include "FileArchiveDialog.hpp"
//#include "UnsavedChangesDialog.hpp"

#include "I18N.hpp"
#include "GUI_App.hpp"
#include "GUI.hpp"
#include "MainFrame.hpp"
#include "ExtraRenderers.hpp"

namespace Slic3r {
namespace GUI {

ArchiveViewModel::ArchiveViewModel(wxWindow* parent)
    :m_parent(parent)
{}
ArchiveViewModel::~ArchiveViewModel()
{}
//wxDataViewItem ArchiveViewModel::AddFolder(wxDataViewItem& parent, wxString name)
//{
//    // TODO
//    return wxDataViewItem(nullptr);
//}
//wxDataViewItem ArchiveViewModel::AddFile(wxDataViewItem& parent, wxString name)
//{
//    ArchiveViewNode* node = new ArchiveViewNode(name);
//
//    wxDataViewItem child((void*)node);
//
//    ItemAdded(parent, child);
//    m_ctrl->Expand(parent);
//    return child;
//}

ArchiveViewNode*  ArchiveViewModel::AddFile(ArchiveViewNode* parent, wxString name)
{
    ArchiveViewNode* node = new ArchiveViewNode(name);
    if (parent != nullptr) {
        parent->get_children().push_back(node);
        node->set_parent(parent);
        parent->set_is_folder(true);
    } else {
        m_top_children.push_back(node);
    }
    
    wxDataViewItem child((void*)node);
    wxDataViewItem parent_item = wxDataViewItem((void*)parent);
    ItemAdded(parent_item, child);
    if (parent)
        m_ctrl->Expand(parent_item);
    return node;
}

wxString ArchiveViewModel::GetColumnType(unsigned int col) const 
{
    if (col == 0)
        return "bool";
    return "string";//"DataViewBitmapText";
}

void ArchiveViewModel::Rescale()
{
    // TODO
}

void  ArchiveViewModel::Delete(const wxDataViewItem& item)
{
    // TODO
    assert(item.IsOk());
    ArchiveViewNode* node = static_cast<ArchiveViewNode*>(item.GetID());
    assert(node->get_parent() != nullptr);
    for (ArchiveViewNode* child : node->get_children())
    {
        Delete(wxDataViewItem((void*)child));
    }
    delete [] node;
}
void  ArchiveViewModel::Clear()
{
    // TODO
}

wxDataViewItem ArchiveViewModel::GetParent(const wxDataViewItem& item) const 
{
    assert(item.IsOk());
    ArchiveViewNode* node = static_cast<ArchiveViewNode*>(item.GetID());
    return wxDataViewItem(node->get_parent());
}
unsigned int ArchiveViewModel::GetChildren(const wxDataViewItem& parent, wxDataViewItemArray& array) const
{
    if (!parent.IsOk()) {
        for (ArchiveViewNode* child : m_top_children) {
            array.push_back(wxDataViewItem((void*)child));
        }
        return m_top_children.size();
    }
       
    ArchiveViewNode* node = static_cast<ArchiveViewNode*>(parent.GetID());
    for (ArchiveViewNode* child : node->get_children()) {
        array.push_back(wxDataViewItem((void*)child));
    }
    return node->get_children().size();
}

void ArchiveViewModel::GetValue(wxVariant& variant, const wxDataViewItem& item, unsigned int col) const 
{
    assert(item.IsOk());
    ArchiveViewNode* node = static_cast<ArchiveViewNode*>(item.GetID());
    if (col == 0) {
        variant = node->get_toggle();
    } else {
        variant = node->get_name();
    }
}

void ArchiveViewModel::untoggle_folders(const wxDataViewItem& item)
{
    assert(item.IsOk());
    ArchiveViewNode* node = static_cast<ArchiveViewNode*>(item.GetID());
    node->set_toggle(false);
    if (node->get_parent())
        untoggle_folders(wxDataViewItem((void*)node->get_parent()));
}

bool ArchiveViewModel::SetValue(const wxVariant& variant, const wxDataViewItem& item, unsigned int col) 
{
    assert(item.IsOk());
    ArchiveViewNode* node = static_cast<ArchiveViewNode*>(item.GetID());
    if (col == 0) {
        node->set_toggle(variant.GetBool());
        // if folder recursivelly check all children
        for (ArchiveViewNode* child : node->get_children()) {
            SetValue(variant, wxDataViewItem((void*)child), col);
        }
        if(!variant.GetBool() && node->get_parent())
            untoggle_folders(wxDataViewItem((void*)node->get_parent()));
    } else {
        node->set_name(variant.GetString());
    }
    m_parent->Refresh();
    return true;
}
bool ArchiveViewModel::IsEnabled(const wxDataViewItem& item, unsigned int col) const 
{
    return true;
}
bool ArchiveViewModel::IsContainer(const wxDataViewItem& item) const 
{
    if(!item.IsOk())
        return true;
    ArchiveViewNode* node = static_cast<ArchiveViewNode*>(item.GetID());
    return node->is_container();
}

ArchiveViewCtrl::ArchiveViewCtrl(wxWindow* parent, wxSize size)
    : wxDataViewCtrl(parent, wxID_ANY, wxDefaultPosition, size, wxDV_VARIABLE_LINE_HEIGHT | wxDV_ROW_LINES
#ifdef _WIN32
        | wxBORDER_SIMPLE
#endif
    )
    //, m_em_unit(em_unit(parent))
{
    wxGetApp().UpdateDVCDarkUI(this);

    m_model = new ArchiveViewModel(parent);
    this->AssociateModel(m_model);
    m_model->SetAssociatedControl(this);
}

ArchiveViewCtrl::~ArchiveViewCtrl()
{
    if (m_model) {
        m_model->Clear();
        m_model->DecRef();
    }
}

FileArchiveDialog::FileArchiveDialog( mz_zip_archive* archive, std::vector<boost::filesystem::path>& selected_paths)
    : DPIDialog(static_cast<wxWindow*>(wxGetApp().mainframe), wxID_ANY, _(L("Archive preview")), wxDefaultPosition,
        wxSize(45 * wxGetApp().em_unit(), 40 * wxGetApp().em_unit()),
        wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX)
    , m_selected_paths (selected_paths)
{
    int em = em_unit();

    wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);


    m_avc = new ArchiveViewCtrl(this, wxSize(60 * em, 30 * em));
    m_avc->AppendToggleColumn(L"\u2714", 0, wxDATAVIEW_CELL_ACTIVATABLE, 6 * em);
    m_avc->AppendTextColumn("filename", 1);
    

    std::vector<ArchiveViewNode*> stack;

    std::function<void(std::vector<ArchiveViewNode*>&, size_t)> reduce_stack = [] (std::vector<ArchiveViewNode*>& stack, size_t size) {
        if (size == 0) {
            stack.clear();
            return;
        }
        while (stack.size() > size)
            stack.pop_back();
    };
    // recursively stores whole structure of file onto function stack and synchoronize with stack object.
    std::function<size_t(boost::filesystem::path&, std::vector<ArchiveViewNode*>&)> adjust_stack = [&adjust_stack, &reduce_stack, &avc = m_avc](boost::filesystem::path& file, std::vector<ArchiveViewNode*>& stack)->size_t {
        size_t struct_size = file.has_parent_path() ? adjust_stack(file.parent_path(), stack) : 0;

        if (stack.size() > struct_size && (file.has_extension() || file.filename().string() != stack[struct_size]->get_name()))
        {
            reduce_stack(stack, struct_size);
        }
        if (!file.has_extension() && stack.size() == struct_size)
            stack.push_back(avc->get_model()->AddFile(stack.empty() ? nullptr : stack.back(), boost::nowide::widen(file.filename().string())));
        return struct_size + 1;
    };

   
    mz_uint num_entries = mz_zip_reader_get_num_files(archive);
    mz_zip_archive_file_stat stat;
    for (mz_uint i = 0; i < num_entries; ++i) {
        if (mz_zip_reader_file_stat(archive, i, &stat)) {
            
            std::string name(stat.m_filename);
            //std::replace(name.begin(), name.end(), '\\', '/');
            boost::filesystem::path path(name);
            if (!path.has_extension())
                continue;
            ArchiveViewNode* parent = nullptr;

            adjust_stack(path, stack);
            if (!stack.empty())
                parent = stack.back();

            m_avc->get_model()->AddFile(parent, boost::nowide::widen(path.filename().string()))->set_fullpath(std::move(path));
        }
    }

    wxBoxSizer* btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton* btn_run = new wxButton(this, wxID_OK, "Open");
    btn_run->Bind(wxEVT_BUTTON, [this](wxCommandEvent& evt) { on_open_button(); });
    btn_sizer->Add(btn_run, 0, wxLEFT | wxRIGHT);

    //ScalableButton* cancel_btn = new ScalableButton(this, wxID_CANCEL, "cross", _L("Cancel"), wxDefaultSize, wxDefaultPosition, wxBORDER_DEFAULT, true, 24);
    //btn_sizer->Add(cancel_btn, 1, wxLEFT | wxRIGHT, 5);
    //cancel_btn->Bind(wxEVT_BUTTON, [this](wxEvent&) { this->EndModal(wxID_CANCEL); });

    topSizer->Add(m_avc, 1, wxEXPAND | wxALL, 10);
    topSizer->Add(btn_sizer, 0, wxEXPAND | wxALL, 10);
    this->SetMinSize(wxSize(80 * em, 30 * em));
    this->SetSizer(topSizer);
}  

void FileArchiveDialog::on_dpi_changed(const wxRect& suggested_rect)
{
    int em = em_unit();

    //msw_buttons_rescale(this, em, { wxID_CANCEL, m_save_btn_id, m_move_btn_id, m_continue_btn_id });
    //for (auto btn : { m_save_btn, m_transfer_btn, m_discard_btn })
    //    if (btn) btn->msw_rescale();

    const wxSize& size = wxSize(70 * em, 30 * em);
    SetMinSize(size);

    //m_tree->Rescale(em);

    Fit();
    Refresh();
}

void FileArchiveDialog::on_open_button()
{
    wxDataViewItemArray top_items;
    m_avc->get_model()->GetChildren(wxDataViewItem(nullptr), top_items);
    
    std::function<void(ArchiveViewNode*)> deep_fill = [&paths = m_selected_paths, &deep_fill](ArchiveViewNode* node){
        if (node == nullptr)
            return;
        if (node->get_children().empty()) {
            if (node->get_toggle()) 
                paths.emplace_back(node->get_fullpath());
        } else { 
            for (ArchiveViewNode* child : node->get_children())
                deep_fill(child);
        }
    };

    for (const auto& item : top_items)
    {
        ArchiveViewNode* node = static_cast<ArchiveViewNode*>(item.GetID());
        deep_fill(node);
    }
    this->EndModal(wxID_OK);
}

} // namespace GUI
} // namespace Slic3r