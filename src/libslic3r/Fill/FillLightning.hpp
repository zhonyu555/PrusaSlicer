#ifndef slic3r_FillLightning_hpp_
#define slic3r_FillLightning_hpp_

#include "FillBase.hpp"

namespace Slic3r {

class PrintObject;

namespace FillLightning {

class Generator;
// To keep the definition of Octree opaque, we have to define a custom deleter.
struct GeneratorDeleter { void operator()(Generator *p); };
using  GeneratorPtr = std::unique_ptr<Generator, GeneratorDeleter>;

GeneratorPtr build_generator(const PrintObject &print_object, const std::function<void()> &throw_on_cancel_callback);

class Filler : public Slic3r::Fill
{
    // Allows usage of desity calibration
    friend class Generator;

public:
    ~Filler() override = default;

    Generator   *generator { nullptr };
protected:
    Fill* clone() const override { return new Filler(*this); }

    void _fill_surface_single(const FillParams              &params,
                              unsigned int                   thickness_layers,
                              const std::pair<float, Point> &direction,
                              ExPolygon                      expolygon,
                              Polylines &polylines_out) override;

    // Let the G-code export reoder the infill lines.
	bool no_sort() const override { return false; }
    // Returns calibration ratio for specified index representing specific density percentage
    virtual float _calibration_density_ratio(size_t index) const override;
};

} // namespace FillAdaptive
} // namespace Slic3r

#endif // slic3r_FillLightning_hpp_
