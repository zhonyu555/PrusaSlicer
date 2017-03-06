// Based on implementation by @platsch

#ifndef slic3r_SlicingAdaptive_hpp_
#define slic3r_SlicingAdaptive_hpp_

#include "Slicing.hpp"
#include "admesh/stl.h"

namespace Slic3r
{

class TriangleMesh;

class SlicingAdaptive
{
public:
	void clear();
	void set_slicing_parameters(SlicingParameters params) { m_slicing_params = params; }
	void add_mesh(const TriangleMesh *mesh) { m_meshes.push_back(mesh); }
	void prepare();
	float layer_height(float z, int &current_facet);
	float horizontal_facet_distance(float z);

private:
	float _layer_height_from_facet(int ordered_id, float scaled_quality_factor);

protected:
	SlicingParameters 					m_slicing_params;

	std::vector<const TriangleMesh*>	m_meshes;
	// Collected faces of all meshes, sorted by raising Z of the bottom most face.
	std::vector<const stl_facet*>		m_faces;
	// Z component of face normals, normalized.
	std::vector<float>					m_face_normal_z;
};

}; // namespace Slic3r

#endif /* slic3r_SlicingAdaptive_hpp_ */
