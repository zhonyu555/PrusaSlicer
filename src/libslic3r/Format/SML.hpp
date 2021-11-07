#ifndef slic3r_Format_SML_hpp_
#define slic3r_Format_SML_hpp_

namespace Slic3r {

class TriangleMesh;
class Model;
class ModelObject;

// Load an OBJ file into a provided model.
extern bool load_sml(const char *path, TriangleMesh *mesh);
extern bool load_sml(const char *path, Model *model, const char *object_name = nullptr);

extern bool store_sml(const char *path, TriangleMesh *mesh);
extern bool store_sml(const char *path, ModelObject *model);
extern bool store_sml(const char *path, Model *model);

}; // namespace Slic3r

#endif /* slic3r_Format_SML_hpp_ */
