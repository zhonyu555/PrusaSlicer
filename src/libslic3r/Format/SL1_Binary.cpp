#include "SL1_Binary.hpp"
#include "SLA/RasterBase.hpp"
#include "libslic3r/LocalesUtils.hpp"
#include "libslic3r/ClipperUtils.hpp"
#include "libslic3r/BoundingBox.hpp"
#include "libslic3r/Utils.hpp"
#include "libslic3r/ShortestPath.hpp"

#include <limits>
#include <cstdint>
#include <algorithm>
#include <string_view>
using namespace std::literals;

namespace Slic3r {

namespace {

void transform(ExPolygon &ep, const sla::RasterBase::Trafo &tr, const BoundingBox &bb)
{
    if (tr.flipXY) {
        for (auto &p : ep.contour.points) std::swap(p.x(), p.y());
        for (auto &h : ep.holes)
            for (auto &p : h.points) std::swap(p.x(), p.y());
    }

    if (tr.mirror_x){
        for (auto &p : ep.contour.points) p.x() = bb.max.x() - p.x() + bb.min.x();
        for (auto &h : ep.holes)
            for (auto &p : h.points) p.x() = bb.max.x() - p.x() + bb.min.x();
    }

    if (tr.mirror_y){
        for (auto &p : ep.contour.points) p.y() = bb.max.y() - p.y() + bb.min.y();
        for (auto &h : ep.holes)
            for (auto &p : h.points) p.y() = bb.max.y() - p.y() + bb.min.y();
    }
}

struct BinaryWriter {
    std::vector<uint8_t> data;

    void store(unsigned char c) { data.emplace_back(c); }
    void store(coord_t v) {
        reserve_more(4);
        auto *c = reinterpret_cast<const unsigned char*>(&v);
        for (int i = 0; i < 4; ++ i)
            store(c[i]);
    }
    void store(uint32_t v) {
        reserve_more(4);
        auto *c = reinterpret_cast<const unsigned char*>(&v);
        for (int i = 0; i < 4; ++ i)
            store(c[i]);
    }
    void store(float v) {
        reserve_more(4);
        auto *c = reinterpret_cast<const unsigned char*>(&v);
        for (int i = 0; i < 4; ++ i)
            store(c[i]);
    }
    void store_plane(uint32_t v, int plane) {
        auto *c = reinterpret_cast<const unsigned char*>(&v);
        store(c[plane]);
    }

    void reserve_more(size_t more) {
        if (data.capacity() < data.size() + more)
            data.reserve(next_highest_power_of_2(data.size() + more));
    }
};

struct RLEPlaneWriter {
    std::vector<uint8_t> data;

    void store(unsigned char c) { data.emplace_back(c); }

    void store_plane(uint32_t v, int plane) {
        auto *c = reinterpret_cast<const unsigned char*>(&v);
        store(c[plane]);
    }

    void reserve_more(size_t more) {
        if (data.capacity() < data.size() + more)
            data.reserve(next_highest_power_of_2(data.size() + more));
    }

    // RLE compression, compressing zeros effectively.
    void compress_to(BinaryWriter &out) {
        for (int i = 0; i < data.size(); ) {
            int j = i;
            if (data[i] == 0) {
                // Always prefer to emit zeros. How many are there?
                for (; j < data.size() && data[j] == 0; ++ j) ;
                // Store number of zeros minus 1.
                out.store((unsigned char)(j - i - 1));
            } else {
                // Store a chain of non-zeros. Count how many non-zeros should be emitted with a single prefix.
                // Emitting a zero is free, however emitting non-zero requires a prefix, thus emitting some zeros
                // inside a string of non-zeros may be cheaper.
                int max = std::min(int(data.size()), i + 128);
                // Skip all non-zeros, allow single zero exceptions.
                for (; j < max && (data[j] != 0 || (j + 1 < max && data[j + 1] != 0)); ++ j) ;
                // Store number of zeros prefixed with zero minus 1.
                out.store((unsigned char)((1 << 7) + (j - i - 1)));
                // Store the non-zero data (with single zero exceptions).
                out.data.insert(out.data.end(), data.begin() + i, data.begin() + j);
            }
            i = j;
        }
    }
};

struct BitWriter {
    std::vector<uint8_t> data;
    int                  size_last { 0 };

    void store(bool v) {
        if (size_last == 0) {
            reserve_more(1);
            data.emplace_back(0);
            size_last = 8;
        }
        data.back() |= uint8_t(v) << (-- size_last);
    }

    void reserve_more(size_t more) {
        if (data.capacity() < data.size() + more)
            data.reserve(next_highest_power_of_2(data.size() + more));
    }
};

} // namespace

// A fake raster from SVG
class BinaryRaster : public sla::RasterBase {
    // Resolution here will be used for svg boundaries
    BoundingBox     m_bb;
    sla::Resolution m_res;
    Trafo           m_trafo;
    Vec2d           m_sc;

    mutable std::vector<Points>  m_polygons;

    // Saves around 1% of data on a large SLA project by bringing the polygons
    // close one to the other, thus reducing the difference when transitioning
    // between polygons.
    static void reorder_polygons(std::vector<Points> &polygons)
    {
        std::vector<Point> centers;
        centers.reserve(polygons.size());
        for (const Points &pts : polygons)
            centers.emplace_back(BoundingBox(pts).center());
        Point zero { 0, 0 };
        std::vector<size_t> ordering = chain_points(centers, &zero);
        std::vector<Points> sorted;
        sorted.reserve(polygons.size());
        for (size_t i = 0; i < polygons.size(); ++ i)
            sorted.emplace_back(std::move(polygons[ordering[i]]));
        polygons = std::move(sorted);
    }

public:
    BinaryRaster(const BoundingBox &svgarea, sla::Resolution res, Trafo tr = {})
        : m_bb{svgarea}
        , m_res{res}
        , m_trafo{tr}
        , m_sc{double(m_res.width_px) / m_bb.size().x(), double(m_res.height_px) / m_bb.size().y()}
    {
    }

    void draw(const ExPolygon& poly) override
    {
        auto cpoly = poly;

        double tol = std::min(m_bb.size().x() / double(m_res.width_px),
                              m_bb.size().y() / double(m_res.height_px));

    ExPolygons cpolys = poly.simplify(tol);

        for (auto &cpoly : cpolys) {
            transform(cpoly, m_trafo, m_bb);

            for (auto &p : cpoly.contour.points)
                p = {std::round(p.x() * m_sc.x()), std::round(p.y() * m_sc.y())};
            m_polygons.emplace_back(std::move(cpoly.contour.points));

            for (auto &h : cpoly.holes) {
                for (auto &p : h)
                    p = {std::round(p.x() * m_sc.x()), std::round(p.y() * m_sc.y())};
                m_polygons.emplace_back(std::move(h.points));
            }
        }
    }

    Trafo trafo() const override { return m_trafo; }

    sla::EncodedRaster encode(sla::RasterEncoder /*encoder*/) const override
    {
        reorder_polygons(m_polygons);

        BinaryWriter writer;
        writer.store(unscaled<float>(m_bb.size().x()));
        writer.store(unscaled<float>(m_bb.size().y()));
        writer.store(uint32_t(m_res.width_px));
        writer.store(uint32_t(m_res.height_px));

#if 1
        static constexpr const int num_planes_raw = 1;
        RLEPlaneWriter plane_writer;

        writer.store(uint32_t(m_polygons.size()));
        for (int plane = 0; plane < num_planes_raw; ++ plane)
            for (const Points &poly : m_polygons)
                writer.store_plane(uint32_t(poly.size()), plane);
        for (int plane = 0; plane < num_planes_raw; ++ plane) {
            plane_writer.data.clear();
            for (const Points &poly : m_polygons)
                plane_writer.store_plane(uint32_t(poly.size()), plane);
            plane_writer.compress_to(writer);
        }

        BitWriter signs;
        {
            coord_t prev = 0;
            for (Points &poly : m_polygons)
                for (Point &pt : poly) {
                    coord_t dif = pt.x() - prev;
                    prev = pt.x();
                    signs.store(dif < 0);
                    pt.x() = std::abs(dif);
                }
            prev = 0;
            for (Points &poly : m_polygons)
                for (Point &pt : poly) {
                    coord_t dif = pt.y() - prev;
                    prev = pt.y();
                    signs.store(dif < 0);
                    pt.y() = std::abs(dif);
                }
        }
        append(writer.data, std::move(signs.data));

        for (int plane = 0; plane < num_planes_raw; ++ plane)
            for (const Points &poly : m_polygons)
                for (const Point &pt : poly)
                    writer.store_plane(pt.x(), plane);
        for (int plane = num_planes_raw; plane < 4; ++ plane) {
            plane_writer.data.clear();
            for (const Points &poly : m_polygons)
                for (const Point &pt : poly)
                    plane_writer.store_plane(pt.x(), plane);
            plane_writer.compress_to(writer);
        }
        for (int plane = 0; plane < num_planes_raw; ++ plane)
            for (const Points &poly : m_polygons)
                for (const Point &pt : poly)
                    writer.store_plane(pt.y(), plane);
        for (int plane = num_planes_raw; plane < 4; ++ plane) {
            plane_writer.data.clear();
            for (const Points &poly : m_polygons)
                for (const Point &pt : poly)
                    plane_writer.store_plane(pt.y(), plane);
            plane_writer.compress_to(writer);
        }
#else
        Point prev { 0, 0 };
        for (Points &poly : m_polygons)
            for (Point &pt : poly) {
                Point dif = pt - prev;
                prev = pt;
                pt = dif;
            }

        writer.store(uint32_t(m_polygons.size()));
        for (const Points &poly : m_polygons)
            writer.store(uint32_t(poly.size()));

        for (const Points &poly : m_polygons)
            for (const Point &pt : poly) {
                writer.store(pt.x());
                writer.store(pt.y());
            }
#endif

        return sla::EncodedRaster{std::move(writer.data), "bin"};
    }
};

std::unique_ptr<sla::RasterBase> SL1_BinaryArchive::create_raster() const
{
    auto w = cfg().display_width.getFloat();
    auto h = cfg().display_height.getFloat();

    float precision_nm = scaled<float>(cfg().sla_output_precision.getFloat());
    size_t res_x = std::round(scaled(w) / precision_nm);
    size_t res_y = std::round(scaled(h) / precision_nm);

    std::array<bool, 2> mirror;

    mirror[X] = cfg().display_mirror_x.getBool();
    mirror[Y] = cfg().display_mirror_y.getBool();

    auto ro = cfg().display_orientation.getInt();
    sla::RasterBase::Orientation orientation =
        ro == sla::RasterBase::roPortrait ? sla::RasterBase::roPortrait :
                                            sla::RasterBase::roLandscape;

    if (orientation == sla::RasterBase::roPortrait) {
        std::swap(w, h);
        std::swap(res_x, res_y);
    }

    BoundingBox svgarea{{0, 0}, {scaled(w), scaled(h)}};

    sla::RasterBase::Trafo tr{orientation, mirror};

    // Gamma does not really make sense in an svg, right?
    // double gamma = cfg().gamma_correction.getFloat();
    return std::make_unique<BinaryRaster>(svgarea, sla::Resolution{res_x, res_y}, tr);
}

sla::RasterEncoder SL1_BinaryArchive::get_encoder() const
{
    return nullptr;
}

} // namespace Slic3r
