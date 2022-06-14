#ifndef SRC_SLIC3R_UTILS_FIXMODELBYTETRAHEDRONS_HPP_
#define SRC_SLIC3R_UTILS_FIXMODELBYTETRAHEDRONS_HPP_

#include <string>
#include <wx/string.h>

class wxProgressDialog;

namespace Slic3r {

class Model;
class ModelObject;
class Print;

bool fix_model_by_tetrahedrons(ModelObject &model_object, int volume_idx, wxProgressDialog &progress_dlg,
        const wxString &msg_header, std::string &fix_result);
}

#endif /* SRC_SLIC3R_UTILS_FIXMODELBYTETRAHEDRONS_HPP_ */
