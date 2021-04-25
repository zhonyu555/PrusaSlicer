#include "../ClipperUtils.hpp"
#include "../ShortestPath.hpp"
#include "../Surface.hpp"

#include "Fill3DHoneycomb.hpp"

namespace Slic3r {

  template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
  }
  
/*
Creates a contiguous sequence of points at a specified height that make
up a horizontal slice of the edges of a space filling truncated
octahedron tesselation. The octahedrons are oriented so that the
square faces are in the horizontal plane with edges parallel to the X
and Y axes.

Credits: David Eccles (gringer).
*/

// triangular wave function
// this has period (gridSize * 2), and amplitude (gridSize / 2),
// with triWave(pos = 0) = 0
static coordf_t triWave(coordf_t pos, coordf_t gridSize)
{
  float t = (pos / (gridSize * 2)) + 0.25; // convert relative to grid size
  t = t - (int)t; // extract fractional part
  return((1. - abs(t * 8. - 4.)) * (gridSize / 4.) + (gridSize / 4));
}

// truncated octagonal waveform, with period and offset
// as per the triangular wave function. The Z position adjusts
// the maximum offset [between -(gridSize / 4) and (gridSize / 4)], with a
// period of (gridSize * 2) and troctWave(Zpos = 0) = 0
// this create a stretched truncated octahedron with equally-
// scaled X/Y/Z; so Z is pre-adjusted first by scaling by sqrt(2)
static coordf_t troctWave(coordf_t pos, coordf_t gridSize, coordf_t Zpos)
{
  // note: sqrt(2) is used to convert to a regular truncated octahedron
  coordf_t Zcycle = triWave(Zpos * sqrt(2), gridSize);
  coordf_t perpOffset = Zcycle / 2;
  coordf_t y = triWave(pos, gridSize);
  return((abs(y) > abs(perpOffset)) ?
	 (sgn(y) * perpOffset) :
	 (y * sgn(perpOffset)));
}

// Identify the important points of curve change within a truncated
// octahedron wave (as waveform fraction t):
// 1. Start of wave (always 0.0)
// 2. Transition to upper "horizontal" part
// 3. Transition from upper "horizontal" part
// 4. Transition to lower "horizontal" part
// 5. Transition from lower "horizontal" part
/*    o---o
 *   /     \
 * o/       \
 *           \       /
 *            \     /
 *             o---o
 */
  static std::vector<coordf_t> getCriticalPoints(coordf_t Zpos, coordf_t gridSize)
{
  std::vector<coordf_t> res = {0.};
  // note: sqrt(2) is used to convert to a regular truncated octahedron
  coordf_t normalisedOffset = abs(triWave(Zpos * sqrt(2), gridSize) / (gridSize * 2.));
  // // for debugging: just generate evenly-distributed points
  // for(coordf_t i = 0; i < 2; i += 0.05){
  //   res.push_back(gridSize * i);
  // }
  if(normalisedOffset == 0){
    // don't duplicate points
    res.push_back(gridSize * 1. / 4.);
    res.push_back(gridSize * 3. / 4.);
    res.push_back(gridSize * 1);
    res.push_back(gridSize * (1 + 1. / 4.));
    res.push_back(gridSize * (1 + 3. / 4.));
  } else {
    res.push_back(gridSize * (1. - normalisedOffset * 2.) / 4.);
    res.push_back(gridSize * (1. + normalisedOffset * 2.) / 4.);
    res.push_back(gridSize * (3. - normalisedOffset * 2.) / 4.);
    res.push_back(gridSize * (3. + normalisedOffset * 2.) / 4.);
    res.push_back(gridSize);
    res.push_back(gridSize * (1 + (1. - normalisedOffset * 2.) / 4.));
    res.push_back(gridSize * (1 + (1. + normalisedOffset * 2.) / 4.));
    res.push_back(gridSize * (1 + (3. - normalisedOffset * 2.) / 4.));
    res.push_back(gridSize * (1 + (3. + normalisedOffset * 2.) / 4.));
  }
  return(res);
}

// Generate an array of points that are in the same direction as the
// basic printing line (i.e. Y points for columns, X points for rows)
// Note: a negative offset only causes a change in the perpendicular
// direction
 static std::vector<coordf_t> colinearPoints(const coordf_t Zpos, coordf_t gridSize, std::vector<coordf_t> critPoints,
					     const size_t baseLocation, size_t gridLength)
{
  std::vector<coordf_t> points;
  points.push_back(baseLocation);
  bool print = true;
  for (coordf_t cLoc = 0; cLoc < gridLength; cLoc+= (gridSize)) {
    if(print){
      for(size_t pi = 0; pi < critPoints.size(); pi++){
	points.push_back(baseLocation + cLoc + critPoints[pi]);
      }
    }
    print = !print;
  }
  points.push_back(baseLocation + gridLength);
  return points;
}

// Generate an array of points for the dimension that is perpendicular to
// the basic printing line (i.e. X points for columns, Y points for rows)
  static std::vector<coordf_t> perpendPoints(const coordf_t Zpos, coordf_t gridSize, std::vector<coordf_t> critPoints,
					     const size_t baseLocation, size_t gridLength, coordf_t perpDir)
{
  std::vector<coordf_t> points;
  points.push_back(baseLocation);
  bool print = true;
  //coordf_t Zpos = 0.5 * gridSize;
  for (coordf_t cLoc = 0; cLoc < gridLength; cLoc+= gridSize) {
    float dir = 1.;
    if(print){
      for(size_t pi = 0; pi < critPoints.size(); pi++){
	//points.push_back(baseLocation);
	coordf_t offset = troctWave(critPoints[pi], gridSize, Zpos);
	//coordf_t offset = triWave(critPoints[pi], gridSize);
	points.push_back(baseLocation+(offset * perpDir * dir));
	//dir *= -1;
	//points.push_back(baseLocation+(offset));
      }
    }
    print = !print;
  }
  points.push_back(baseLocation);
  return points;
}

// Trims an array of points to specified rectangular limits. Point
// components that are outside these limits are set to the limits.
static inline void trim(Pointfs &pts, coordf_t minX, coordf_t minY, coordf_t maxX, coordf_t maxY)
{
    for (Vec2d &pt : pts) {
        pt(0) = clamp(minX, maxX, pt(0));
        pt(1) = clamp(minY, maxY, pt(1));
    }
}

static inline Pointfs zip(const std::vector<coordf_t> &x, const std::vector<coordf_t> &y)
{
    assert(x.size() == y.size());
    Pointfs out;
    out.reserve(x.size());
    for (size_t i = 0; i < x.size(); ++ i)
        out.push_back(Vec2d(x[i], y[i]));
    return out;
}

// Generate a set of curves (array of array of 2d points) that describe a
// horizontal slice of a truncated regular octahedron.
static std::vector<Pointfs> makeActualGrid(coordf_t Zpos, coordf_t gridSize, size_t boundsX, size_t boundsY)
{
    std::vector<Pointfs> points;
    std::vector<coordf_t> critPoints = getCriticalPoints(Zpos, gridSize);
  // note: sqrt(2) is used to convert to a regular truncated octahedron
    coordf_t zCycle = fmod((Zpos * sqrt(2)) + gridSize/2, gridSize * 2.) / (gridSize * 2.);
    bool printVert = zCycle < 0.5;
    if (printVert) {
      int perpDir = -1;
      for (size_t x = 0; x <= boundsX; x+= gridSize, perpDir *= -1) {
	  points.push_back(Pointfs());
	  Pointfs &newPoints = points.back();
	  newPoints = zip(
			  perpendPoints(Zpos, gridSize, critPoints, x, boundsY, perpDir), 
			  colinearPoints(Zpos, gridSize, critPoints, 0, boundsY));
	  // trim points to grid edges
	  //trim(newPoints, coordf_t(0.), coordf_t(0.), coordf_t(colsToPrint), coordf_t(rowsToPrint));
	  if (x & 1)
	    std::reverse(newPoints.begin(), newPoints.end());
        }
    } else {
      int perpDir = 1;
      for (size_t y = gridSize; y <= boundsY; y+= gridSize, perpDir *= -1) {
	points.push_back(Pointfs());
	Pointfs &newPoints = points.back();
	newPoints = zip(
			colinearPoints(Zpos, gridSize, critPoints, 0, boundsX),
			perpendPoints(Zpos, gridSize, critPoints, y, boundsX, perpDir));
	// trim points to grid edges
	//trim(newPoints, coordf_t(0.), coordf_t(0.), coordf_t(colsToPrint), coordf_t(rowsToPrint));
	if (y & 1)
	  std::reverse(newPoints.begin(), newPoints.end());
      }
    }
    return points;
}

// Generate a set of curves (array of array of 2d points) that describe a
// horizontal slice of a truncated regular octahedron with a specified
// grid square size.
// gridWidth and gridHeight define the width and height of the bounding box respectively
static Polylines makeGrid(coordf_t z, coordf_t gridSize, coordf_t boundWidth, coordf_t boundHeight, bool fillEvenly)
{
  std::vector<Pointfs> polylines =
    makeActualGrid(z, gridSize, boundWidth, boundHeight);
  Polylines result;
  result.reserve(polylines.size());
  for (std::vector<Pointfs>::const_iterator it_polylines = polylines.begin();
       it_polylines != polylines.end(); ++ it_polylines) {
    result.push_back(Polyline());
    Polyline &polyline = result.back();
    for (Pointfs::const_iterator it = it_polylines->begin(); it != it_polylines->end(); ++ it)
      polyline.points.push_back(Point(coord_t((*it)(0)), coord_t((*it)(1))));
  }
  return result;
}

// FillParams has the following useful information:
// density <0 .. 1>  [proportion of space to fill]
// anchor_length     [???]
// anchor_length_max [???]
// dont_connect()    [avoid connect lines]
// dont_adjust       [avoid filling space evenly]
// monotonic         [fill strictly left to right]
// complete          [complete each loop]
  
void Fill3DHoneycomb::_fill_surface_single(
    const FillParams                &params, 
    unsigned int                     thickness_layers,
    const std::pair<float, Point>   &direction, 
    ExPolygon                        expolygon,
    Polylines                       &polylines_out)
{
    // no rotation is supported for this infill pattern
    BoundingBox bb = expolygon.contour.bounding_box();

    // adjustment to account for the additional distance of octagram curves
    // note: this only strictly applies for a rectangular area where the total
    //       Z travel distance is a multiple of the spacing... but it should
    //       be at least better than the prevous estimate which assumed straight
    //       lines
    // = 4 * integrate(func=4*x(sqrt(2) - 1) + 1, from=0, to=0.25)
    // = (sqrt(2) + 1) / 2 [... I think]
    coordf_t gridSize = (scale_(this->spacing) * ((sqrt(2) + 1.) / 2.) / params.density);

    // align bounding box to a multiple of our honeycomb grid module
    // (a module is 2*$gridSize since one $gridSize half-module is 
    // growing while the other $gridSize half-module is shrinking)
    bb.merge(align_to_grid(bb.min, Point(gridSize*2, gridSize*2)));
    
    // generate pattern
    Polylines   polylines = makeGrid(
        scale_(this->z),
        gridSize,
        bb.size()(0),
        bb.size()(1),
	!params.dont_adjust);
    
    // move pattern in place
	for (Polyline &pl : polylines)
		pl.translate(bb.min);

    // clip pattern to boundaries, chain the clipped polylines
    polylines = intersection_pl(polylines, to_polygons(expolygon));

    // connect lines if needed
    if (params.dont_connect() || polylines.size() <= 1)
        append(polylines_out, chain_polylines(std::move(polylines)));
    else
        this->connect_infill(std::move(polylines), expolygon, polylines_out, this->spacing, params);
}

} // namespace Slic3r
