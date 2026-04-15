#include "art.h"

#include "globals.h"

volatile bool g_running = false;

bool g_circles = false;

UINT g_num_shapes = 1;

unsigned long g_delay = 1000UL;

DWORD WINAPI ArtThread(LPVOID pvoid) {
  UNREFERENCED_PARAMETER(pvoid);
  if (mainHwnd == nullptr) {
    return 0x00000001;
  }
  int xLeft   = 0;
  int xRight  = 0;
  int yTop    = 0;
  int yBottom = 0;
  int iRed   = 0;
  int iGreen = 0;
  int iBlue  = 0;

  const bool draw_ellipses = g_circles;
  const unsigned int num_shapes = g_num_shapes;
  const unsigned long paint_delay = g_delay;

  HBRUSH hBrush          = nullptr;
  const HPEN hOldPen     = reinterpret_cast<HPEN>(GetStockObject(BLACK_PEN));
  HDC hdc                = nullptr;
  std::random_device rng;
  std::uniform_int_distribution<int> colorDist(0, 255);
  while (g_running) {
    Sleep(paint_delay); // Sleep before first update and between updates
    if (cxClient == 0 && cyClient == 0) {
      continue; // Window is minimized; wait for restore
    }
    hdc = GetDC(mainHwnd);
    std::uniform_int_distribution<int> xDist(0, cxClient - 1);
    std::uniform_int_distribution<int> yDist(0, cyClient - 1);
    for (unsigned int i = 0; i < num_shapes; i++) {
      // Randomize positions
      xLeft   = xDist(rng);
      xRight  = xDist(rng);
      yTop    = yDist(rng);
      yBottom = yDist(rng);
      // Randomize colors
      iRed    = colorDist(rng);
      iGreen  = colorDist(rng);
      iBlue   = colorDist(rng);
      // Outline is the complementary (opposite) color of the fill
      HPEN hPen = CreatePen(PS_SOLID, 1, RGB(255 - iRed, 255 - iGreen, 255 - iBlue));
      // Create brush from above colors
      hBrush = CreateSolidBrush(RGB(iRed, iGreen, iBlue));
      SelectObject(hdc, hPen);
      SelectObject(hdc, hBrush);
      // Paint shape(s)
      if (draw_ellipses) {
        Ellipse(hdc, std::min(xLeft, xRight), std::min(yTop, yBottom),
                std::max(xLeft, xRight), std::max(yTop, yBottom));
      } else {
        Rectangle(hdc, std::min(xLeft, xRight), std::min(yTop, yBottom),
                  std::max(xLeft, xRight), std::max(yTop, yBottom));
      }
      SelectObject(hdc, hOldPen);
      DeleteObject(hPen);
      DeleteObject(hBrush);
    }
    // Release HDC after all shapes are drawn
    ReleaseDC(mainHwnd, hdc);
  }
  return 0x00000000;
}
