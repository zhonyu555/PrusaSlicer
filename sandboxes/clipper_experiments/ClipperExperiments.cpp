#include <fstream>

#include <libslic3r/ClipperUtils.hpp>
#include <libslic3r/SVG.hpp>

using namespace Slic3r;

inline ExPolygon rectangle(double w, double h)
{
    return {scaled(Vec2d(0, 0)), scaled(Vec2d(w, 0)), scaled(Vec2d(w, h)),
            scaled(Vec2d(0, h))};
}

inline ExPolygon rectangle(double a) { return rectangle(a, a); }
inline ExPolygon &&translate(ExPolygon &&p, double x, double y)
{
    p.translate(scaled(Vec2d(x, y)));
    return std::move(p);
}

constexpr const char * FILLTYPE_STR[] = {
    "pftEvenOdd", "pftNonZero", "pftPositive", "pftNegative"
};

constexpr const char * PTYPE_STR[] = { "ptSubject", "ptClip" };

constexpr const char * CLIPTYPE_STR[] = { "ctIntersection", "ctUnion", "ctDifference", "ctXor" };

constexpr const char * JOINTYPE_STR[] = { "jtSquare", "jtRound", "jtMiter"};

void eval_clipping(const char *prefix, const ExPolygons &a, const ExPolygons &b)
{
    BoundingBox bb = get_extents(a); bb.merge(get_extents(b));

    SVG svgin(std::string(prefix) + "_input_clipping.svg", bb);
    svgin.draw(a, "green", 0.5);
    svgin.draw(b, "red", 0.5);
    svgin.draw_outline(a, "black", "blue");
    svgin.draw_outline(b, "yellow", "magenta");
    svgin.Close();

    for (auto bPolyType : {ClipperLib::ptSubject, ClipperLib::ptClip})
        for (auto cliptype  : {ClipperLib::ctIntersection, ClipperLib::ctUnion, ClipperLib::ctDifference, ClipperLib::ctXor})
            for (auto filltype  : {ClipperLib::pftEvenOdd, ClipperLib::pftNegative, ClipperLib::pftNonZero, ClipperLib::pftPositive}) {
                ClipperLib::Clipper clipper;

                for (auto &p : a) {
                    clipper.AddPath(p.contour.points, ClipperLib::ptSubject, true);
                    for (auto &h : p.holes)
                        clipper.AddPath(h.points, ClipperLib::ptSubject, true);
                }

                for (auto &p : b) {
                    clipper.AddPath(p.contour.points, bPolyType, true);
                    for (auto &h : p.holes)
                        clipper.AddPath(h.points, bPolyType, true);
                }

                ClipperLib::PolyTree tree;
                clipper.Execute(cliptype, tree, filltype);
                SVG svg{std::string{prefix} + "_clipping_" + CLIPTYPE_STR[cliptype] + "_" + PTYPE_STR[bPolyType] + "_" + FILLTYPE_STR[filltype] + ".svg", bb};
                ExPolygons res = PolyTreeToExPolygons(std::move(tree));
                svg.draw(res, "green");
                svg.draw_outline(res);
            }
}

constexpr const char * WEBPREFIX = "";//"https://cfl.prusa3d.com/download/attachments/73269824/";

void generate_clipping_report(std::fstream &report, const char * prefix)
{
    report << "Clipping input is: ![img](" << WEBPREFIX << prefix
           << "_input_clipping.svg)\n**A** is filled with green and outlined "
              "with black and blue (holes), **B** is filled with red and outined "
              "with yellow and magenta (holes)\n"
           << std::endl;

    for (auto ptype : PTYPE_STR) {
        report << "When B is of type " << ptype << "\n\n";

        report << "|Operation";
        for (auto filltype : FILLTYPE_STR) report << "|" << filltype;
        report << "|\n";

        for (size_t i = 0; i < std::size(FILLTYPE_STR) + 1; ++i)
            report << "|----";
        report << "|\n";

        for (auto cliptype  : CLIPTYPE_STR) {
            report << "|" << cliptype;
            for (auto filltype : FILLTYPE_STR)
                report << "|![img](" << WEBPREFIX << prefix << "_clipping_" << cliptype << "_" << ptype << "_" << filltype << ".svg)";
            report << "|\n";
        }

        report << std::endl;
    }
}

void eval_offsetting(const char *prefix, const ExPolygons &polys)
{
    auto bb = get_extents(polys);

    SVG svgin(std::string(prefix) + "_input_offsetting.svg", bb);
    svgin.draw(polys, "green", 0.5);
    svgin.draw_outline(polys);
    svgin.Close();

    for (auto delta : {-1., 0., 1.})
        for (auto jointype : {ClipperLib::jtMiter, ClipperLib::jtRound, ClipperLib::jtSquare}) {
            ClipperLib::ClipperOffset offset;

            for (const ExPolygon &p : polys) {
                offset.AddPath(p.contour.points, jointype, ClipperLib::etClosedPolygon);
                for (auto &h : p.holes)
                    offset.AddPath(h.points, jointype, ClipperLib::etClosedPolygon);
            }

            ClipperLib::PolyTree tree;
            offset.Execute(tree, scaled(delta));

            SVG svg{std::string{prefix} + "_offsetting_delta_" + std::to_string(delta) + "_" + JOINTYPE_STR[jointype] + ".svg", bb};
            ExPolygons res = PolyTreeToExPolygons(std::move(tree));
            svg.draw(res, "green");
            svg.draw_outline(res);
        }
}

void generate_offsetting_report(std::fstream &report, const char * prefix)
{
    report << "Offsetting input is: ![img](" << WEBPREFIX << prefix << "_input_offsetting.svg)\n" << std::endl;

    report << "|Delta";
    for (auto jointype : JOINTYPE_STR) report << "|" << jointype;
    report << "|\n";

    for (size_t i = 0; i < std::size(JOINTYPE_STR) + 1; ++i)
        report << "|----";
    report << "|\n";

    for (auto delta  : {-1., 0., 1.}) {
        report << "|" << delta;
        for (auto jtype : JOINTYPE_STR)
            report << "|![img](" << WEBPREFIX << prefix << "_offsetting_delta_" << std::to_string(delta) << "_" << jtype << ".svg)";

        report << "|\n";
    }

    report << std::endl;
}

int main()
{
    auto a = rectangle(10.), b = translate(rectangle(10.), 5., 5.);
    a.holes.emplace_back(translate(rectangle(5), 2.5, 2.5));
    b.holes.emplace_back(translate(rectangle(5), 7.5, 7.5));
    std::reverse(a.holes.front().begin(), a.holes.front().end());
    std::reverse(b.holes.front().begin(), b.holes.front().end());
    ExPolygons A = {a, b};

    ExPolygons B = A;
    for (auto &b : B) b.translate(scaled(4.), scaled(-4.));

    std::fstream report{"report.md", std::fstream::out};

    constexpr const char *prefix = "two_squares_with_holes";
    eval_clipping(prefix, A, B);
    generate_clipping_report(report, prefix);
    report << "\n" << std::endl;

    eval_offsetting(prefix, A);
    generate_offsetting_report(report, prefix);
    report << "\n" << std::endl;

//    ExPolygons C = A;
//    for (auto &c : C) c.holes.clear();
//    auto c = translate(rectangle(1.5, 1.5), 4.25, 4.25);
//    eval_offsetting("holed_square_with_rect_inside", {a, c});
//    generate_offsetting_report(report, "holed_square_with_rect_inside");
//    report << "\n" << std::endl;

    return 0;
}
