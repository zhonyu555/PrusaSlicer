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

void eval_clipping(const char *prefix, const ExPolygon &a, const ExPolygon &b)
{
    BoundingBox bb{a}; bb.merge(BoundingBox{b});

    SVG svgin(std::string(prefix) + "_input_clipping.svg", bb);
    svgin.draw_outline(a);
    svgin.draw_outline(b);
    svgin.Close();

    for (auto bPolyType : {ClipperLib::ptSubject, ClipperLib::ptClip})
        for (auto cliptype  : {ClipperLib::ctIntersection, ClipperLib::ctUnion, ClipperLib::ctDifference, ClipperLib::ctXor})
            for (auto filltype  : {ClipperLib::pftEvenOdd, ClipperLib::pftNegative, ClipperLib::pftNonZero, ClipperLib::pftPositive}) {
                ClipperLib::Clipper clipper;

                clipper.AddPath(a.contour.points, ClipperLib::ptSubject, true);
                for (auto &h : a.holes)
                    clipper.AddPath(h.points, ClipperLib::ptSubject, true);

                clipper.AddPath(b.contour.points, bPolyType, true);
                for (auto &h : b.holes)
                    clipper.AddPath(h.points, bPolyType, true);

                ClipperLib::PolyTree tree;
                clipper.Execute(cliptype, tree, filltype);
                SVG svg{std::string{prefix} + "_clipping_" + CLIPTYPE_STR[cliptype] + "_" + PTYPE_STR[bPolyType] + "_" + FILLTYPE_STR[filltype] + ".svg", bb};
                svg.draw(PolyTreeToExPolygons(std::move(tree)), "green");
            }
}

void generate_clipping_report(std::fstream &report, const char * prefix)
{
    report << "Clipping input is: ![img](" << prefix << "_input_clipping.svg)\nA to the left, B to the right\n" << std::endl;
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
                report << "|![img](" << prefix << "_clipping_" << cliptype << "_" << ptype << "_" << filltype << ".svg)";
            report << "|\n";
        }

        report << std::endl;
    }
}

void eval_offsetting(const char *prefix, const ExPolygons &polys)
{
    auto bb = get_extents(polys);

    SVG svgin(std::string(prefix) + "_input_offsetting.svg", bb);
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
            svg.draw(PolyTreeToExPolygons(std::move(tree)), "green");
        }
}

void generate_offsetting_report(std::fstream &report, const char * prefix)
{
    report << "Offsetting input is: ![img](" << prefix << "_input_offsetting.svg)" << std::endl;

    report << "|Delta";
    for (auto jointype : JOINTYPE_STR) report << "|" << jointype;
    report << "|\n";

    for (size_t i = 0; i < std::size(JOINTYPE_STR) + 1; ++i)
        report << "|----";
    report << "|\n";

    for (auto delta  : {-1., 0., 1.}) {
        report << "|" << delta;
        for (auto jtype : JOINTYPE_STR)
            report << "|![img](" << prefix << "_offsetting_delta_" << std::to_string(delta) << "_" << jtype << ".svg)";

        report << "|\n";
    }

    report << std::endl;
}

int main()
{
    auto a = rectangle(10.), b = translate(rectangle(10.), 5., 5.);
    std::fstream report{"report.md", std::fstream::out};

    eval_offsetting("two_squares_implicit_union", {a, b});
    generate_offsetting_report(report, "two_squares_implicit_union");

    ExPolygons ab = union_ex({a, b});
    eval_offsetting("two_squares_explicit_union", ab);
    generate_offsetting_report(report, "two_squares_explicit_union");

    eval_clipping("two_squares", a, b);

    generate_clipping_report(report, "two_squares");
    report << "\n" << std::endl;

    a.holes.emplace_back(translate(rectangle(5), 2.5, 2.5));
    std::reverse(a.holes.front().begin(), a.holes.front().end());

    b.holes.emplace_back(translate(rectangle(5.), 7.5, 7.5));
    std::reverse(b.holes.front().begin(), b.holes.front().end());

    eval_clipping("two_squares_with_holes", a, b);
    generate_clipping_report(report, "two_squares_with_holes");

    return 0;
}
