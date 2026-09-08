//----------------------------------------------------------------------
// test_present_geometry.cpp
//----------------------------------------------------------------------
//
// spritectl_window_to_game_coords (Client/SpriteLib/SpriteLibBackendSDL.cpp)
// on a high-DPI display. The letterbox rectangle a present records is in
// renderer output pixels; a mouse event is in window points; on a Retina
// Mac with SDL_WINDOW_ALLOW_HIGHDPI the output is twice the window, and
// a mapping that read the point as a pixel landed every click at half
// its distance from the top-left corner (docs/linux-macos-port-
// assessment-2026-09-07.md, area G). No machine that runs these tests
// has such a display and SDL's dummy driver cannot fake one, so the
// geometry is set by hand through spritectl_set_present_geometry - the
// values a present would have recorded there, computed the same way it
// computes them.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "SpriteLibBackend.h"

namespace {

// (x, y) in window points through the mapping.
struct Point { int x; int y; };

Point Map(int x, int y)
{
	spritectl_window_to_game_coords(&x, &y);
	return { x, y };
}

} // namespace

TEST(PresentGeometry, RetinaWindowMapsPointsThroughThePixelRatio)
{
	// 1024x768 game frame in a 1024x768-point window at 2x: the output is
	// 2048x1536 and the frame fills it. A point is two pixels, and a game
	// pixel is two output pixels, so the mapping is the identity - the
	// old code, which skipped the first step, halved everything.
	spritectl_set_present_geometry(1024, 768, 2048, 1536, 0, 0, 2048, 1536, 1024, 768);
	Point p = Map(512, 384);
	CHECK_EQ(512, p.x);
	CHECK_EQ(384, p.y);
	p = Map(1023, 767);
	CHECK_EQ(1023, p.x);
	CHECK_EQ(767, p.y);
	p = Map(0, 0);
	CHECK_EQ(0, p.x);
	CHECK_EQ(0, p.y);
}

TEST(PresentGeometry, RetinaLetterboxLandsInTheFrameNotTheBar)
{
	// 800x600 frame in a 1600x1000-point window at 2x: output 3200x2000,
	// the largest aspect-preserving frame is 2666x2000 (16.16 fixed
	// point, as the present computes it) at x = 267. The window's centre
	// (800, 500) is the frame's centre (400, 300); a point in the left
	// bar clamps to column 0; the right edge of the frame is the last
	// column.
	spritectl_set_present_geometry(1600, 1000, 3200, 2000, 267, 0, 2666, 2000, 800, 600);
	Point p = Map(800, 500);
	CHECK_EQ(400, p.x);
	CHECK_EQ(300, p.y);
	p = Map(50, 500);
	CHECK_EQ(0, p.x);
	p = Map(1599, 999);
	CHECK_EQ(799, p.x);
	CHECK_EQ(599, p.y);
}

TEST(PresentGeometry, NonRetinaWindowIsUnchangedByTheRatio)
{
	// The same letterbox on an ordinary display: window and output agree
	// and the mapping is what it was before the ratio step existed.
	spritectl_set_present_geometry(1600, 1000, 1600, 1000, 133, 0, 1333, 1000, 800, 600);
	Point p = Map(133 + 666, 500);
	CHECK_EQ(399, p.x);
	CHECK_EQ(300, p.y);
	p = Map(0, 0);
	CHECK_EQ(0, p.x);
	CHECK_EQ(0, p.y);
}

TEST(PresentGeometry, NoWindowSizeMeansOutputSize)
{
	// A renderer without a window (the software renderer the other tests
	// present to) records no window size; the ratio step is skipped
	// rather than dividing by zero.
	spritectl_set_present_geometry(0, 0, 80, 64, 0, 0, 80, 64, 16, 16);
	Point p = Map(40, 32);
	CHECK_EQ(8, p.x);
	CHECK_EQ(8, p.y);
}
