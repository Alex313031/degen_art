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

  HBRUSH hBrush      = nullptr;
  // BLACK_PEN is a stock GDI object (always available, never needs DeleteObject).
  // We save it here so we can restore it into the DC after each shape, which is
  // required before we can safely delete our custom pen.
  const HPEN hOldPen = reinterpret_cast<HPEN>(GetStockObject(BLACK_PEN));
  HDC hdc            = nullptr;
  std::random_device rng;
  std::uniform_int_distribution<int> colorDist(0, 255);
  while (g_running) {
    Sleep(paint_delay); // Sleep before first update and between updates
    if (cxClient == 0 && cyClient == 0) {
      continue; // Window is minimized; wait for restore
    }
    // Acquire the critical section before touching g_hdcMem. The main thread
    // also holds this lock in WM_PAINT and RecreateBackBuffer, so this ensures
    // we never draw into the back buffer while it is being read or replaced.
    EnterCriticalSection(&g_paintCS);
    if (g_hdcMem != nullptr) {
      // Rebuild distributions each iteration in case the window was resized
      // while we were sleeping, changing cxClient/cyClient.
      std::uniform_int_distribution<int> xDist(0, cxClient - 1);
      std::uniform_int_distribution<int> yDist(0, cyClient - 1);
      for (unsigned int i = 0; i < num_shapes; i++) {
        // Randomize positions
        xLeft   = xDist(rng);
        xRight  = xDist(rng);
        yTop    = yDist(rng);
        yBottom = yDist(rng);
        // Randomize fill color
        iRed    = colorDist(rng);
        iGreen  = colorDist(rng);
        iBlue   = colorDist(rng);
        // Outline is the complementary (opposite) color of the fill.
        // Subtracting each channel from 255 inverts it, maximizing contrast.
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(255 - iRed, 255 - iGreen, 255 - iBlue));
        hBrush    = CreateSolidBrush(RGB(iRed, iGreen, iBlue));
        // SelectObject makes the pen/brush active in the DC. The shape drawing
        // functions below will use whatever pen and brush are currently selected.
        SelectObject(g_hdcMem, hPen);
        SelectObject(g_hdcMem, hBrush);
        // Draw the shape into the back buffer (g_hdcMem), not the screen.
        // min/max ensure the coordinates are top-left/bottom-right regardless
        // of which random value ended up larger.
        if (draw_ellipses) {
          Ellipse(g_hdcMem, std::min(xLeft, xRight), std::min(yTop, yBottom),
                  std::max(xLeft, xRight), std::max(yTop, yBottom));
        } else {
          Rectangle(g_hdcMem, std::min(xLeft, xRight), std::min(yTop, yBottom),
                    std::max(xLeft, xRight), std::max(yTop, yBottom));
        }
        // Restore the stock pen before deleting ours. A GDI object must not be
        // deleted while it is still selected into a DC.
        SelectObject(g_hdcMem, hOldPen);
        DeleteObject(hPen);
        DeleteObject(hBrush);
      }
      // Present the back buffer to the screen. BitBlt does a direct pixel copy
      // (SRCCOPY) of the entire back buffer onto the window's DC. GetDC/ReleaseDC
      // bracket all direct drawing to the window outside of WM_PAINT.
      hdc = GetDC(mainHwnd);
      BitBlt(hdc, 0, 0, cxClient, cyClient, g_hdcMem, 0, 0, SRCCOPY);
      ReleaseDC(mainHwnd, hdc);
    }
    LeaveCriticalSection(&g_paintCS);
    // GdiFlush ensures all batched GDI operations for this thread are submitted
    // to the driver. Required on Windows 10/11 where DWM batches more aggressively
    // and shapes may otherwise not appear until the batch is flushed naturally.
    GdiFlush();
  }
  return 0x00000000;
}

// Creates or replaces the off-screen back buffer to match the current client
// area size. A "back buffer" is an off-screen bitmap we draw into before
// presenting to the screen. This lets WM_PAINT restore any region that gets
// invalidated (e.g. another window dragged over ours) without losing shapes.
//
// A "compatible" DC/bitmap mirrors the pixel format of the real window DC so
// that BitBlt can copy between them without color conversion overhead.
void RecreateBackBuffer(HWND hWnd, int cx, int cy) {
  if (cx <= 0 || cy <= 0 || g_hdcMem == nullptr) return;
  // Borrow the window DC only to query its pixel format for CreateCompatibleBitmap.
  HDC hdcWin = GetDC(hWnd);
  HBITMAP hbmNew = CreateCompatibleBitmap(hdcWin, cx, cy);
  ReleaseDC(hWnd, hdcWin);
  // Hold the lock while swapping the bitmap so the art thread cannot draw into
  // g_hdcMem while we are replacing what it points at.
  EnterCriticalSection(&g_paintCS);
  // SelectObject swaps the new bitmap into the memory DC, making g_hdcMem ready
  // to draw into at the new size. The previously selected bitmap is implicitly
  // deselected and safe to delete.
  SelectObject(g_hdcMem, hbmNew);
  if (g_hbmMem != nullptr) DeleteObject(g_hbmMem);
  g_hbmMem = hbmNew;
  // Fill the fresh bitmap with white so newly exposed areas on resize show a
  // clean background rather than uninitialized (black) pixels.
  RECT rc = { 0, 0, cx, cy };
  FillRect(g_hdcMem, &rc, reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
  LeaveCriticalSection(&g_paintCS);
}
