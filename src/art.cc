#include "art.h"

#include "globals.h"
#include "resource.h"
#include "utils.h"

volatile bool g_running = false; // Global art threads running state
volatile bool g_paused  = false; // Affects g_running, used by IDM_PAUSED

volatile bool g_circles = false; // IDM_ELLIPSES checked in the .rc menu.
volatile bool g_beziers = false; // IDM_BEZIERS checked in the .rc menu.
volatile bool g_lines   = false; // IDM_LINES checked in the .rc menu.
volatile bool g_both    = true;  // IDM_BOTH default checked in the .rc menu.

bool g_monochrome = false; // Whether monochrome colors only is enabled

volatile UINT g_num_shapes = 1; // Initialize to 1, in case something goes wrong at least we draw 1 shape

unsigned long g_delay = 500UL; // Default to same as .rc file (IDM_FAST)

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

  HBRUSH hBrush      = nullptr;
  // BLACK_PEN is a stock GDI object (always available, never needs DeleteObject).
  // We save it here so we can restore it into the DC after each shape, which is
  // required before we can safely delete our custom pen.
  const HPEN hOldPen = reinterpret_cast<HPEN>(GetStockObject(BLACK_PEN));
  HDC hdc            = nullptr;
  std::random_device rng;
  std::uniform_int_distribution<int> colorDist(0, 255);
  // Used for the "both" mode coin toss — each shape independently picks a type.
  std::uniform_int_distribution<int> coinDist(0, 1);
  std::uniform_int_distribution<int> diceDist(0, 5);
  // Fixed palette for bezier or lines strokes — a small set of saturated colors looks
  // cleaner against the busy random shape background than fully random hues.
  static const COLORREF customPalette[] = {
    RGB_BLACK,  RGB_WHITE, RGB_GREY,
    RGB_RED,    RGB_GREEN, RGB_BLUE,
    RGB_YELLOW, RGB_CYAN,  RGB_MAGENTA,
  };
  std::uniform_int_distribution<int> paletteDist(
      0, static_cast<int>(sizeof(customPalette) / sizeof(customPalette[0])) - 1);
  // In monochrome mode we restrict the palette to the first three entries
  // (BLACK, WHITE, GREY) so bezier/line strokes stay tonally consistent with
  // the monochrome shape fills rather than injecting saturated color.
  std::uniform_int_distribution<int> monoDist(0, 2);
  // Line direction picker: 0=horizontal, 1=vertical, 2=diag "\", 3=diag "/".
  std::uniform_int_distribution<int> dirDist(0, 3);
  while (g_running) {
    const unsigned int num_shapes = g_num_shapes;
    // Block until WM_TIMER signals g_hDrawEvent. The event is auto-reset, so
    // it returns to non-signalled immediately after this call returns, making
    // the thread wait again on the next iteration. If g_hDrawEvent is null
    // (shutdown race), exit cleanly.
    if (g_hDrawEvent == nullptr ||
        WaitForSingleObject(g_hDrawEvent, INFINITE) != WAIT_OBJECT_0) {
      break;
    }
    if (!g_running) break; // event was signalled to unblock shutdown
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
        if (g_monochrome) {
          // A single random value applied to all channels gives a gray shade
          // uniformly distributed between pure black and pure white.
          iRed = iGreen = iBlue = colorDist(rng);
        } else {
          iRed    = colorDist(rng);
          iGreen  = colorDist(rng);
          iBlue   = colorDist(rng);
        }
        // Determine outline color
        COLORREF outlineColor;
        if (g_monochrome) {
          // Dark-to-mid gray (0-128) → white outline; light gray (129-255) →
          // black outline. iRed == iGreen == iBlue in monochrome mode so any
          // channel works as the brightness value.
          outlineColor = (iRed <= 128) ? RGB_WHITE : RGB_BLACK;
        } else {
          // Complementary color: invert each channel, maximizing contrast.
          outlineColor = RGB(255 - iRed, 255 - iGreen, 255 - iBlue);
        }
        HPEN hPen = CreatePen(PS_SOLID, 1, outlineColor);
        hBrush    = CreateSolidBrush(RGB(iRed, iGreen, iBlue));
        // SelectObject makes the pen/brush active in the DC. The shape drawing
        // functions below will use whatever pen and brush are currently selected.
        SelectObject(g_hdcMem, hPen);
        SelectObject(g_hdcMem, hBrush);
        // Determine shape type for this individual shape. g_circles and g_both
        // are read fresh each iteration so menu changes take effect immediately
        // without restarting the thread.
        // - IDM_BOTH:       coin toss per shape
        // - IDM_ELLIPSES:   g_circles=true,  g_both=false → always ellipse
        // - IDM_RECTANGLES: g_circles=false, g_both=false → always rectangle
        const bool use_ellipse = g_both ? (coinDist(rng) != 0) : g_circles;
        // Only use beziers if enabled, with a 1-in-6 chance per shape.
        // diceDist yields 0..5 uniformly, so matching 0 gives exactly 1/6.
        const bool use_beziers = g_beziers ? (diceDist(rng) == 0) : false;
        const bool use_lines   = g_lines   ? (diceDist(rng) == 0) : false;
        // Draw the shape into the back buffer (g_hdcMem), not the screen.
        // min/max ensure the coordinates are top-left/bottom-right regardless
        // of which random value ended up larger.
        if (use_ellipse) {
          Ellipse(g_hdcMem, std::min(xLeft, xRight), std::min(yTop, yBottom),
                  std::max(xLeft, xRight), std::max(yTop, yBottom));
        } else {
          Rectangle(g_hdcMem, std::min(xLeft, xRight), std::min(yTop, yBottom),
                    std::max(xLeft, xRight), std::max(yTop, yBottom));
        }
        // Now draw a random bezier on top, if this iteration rolled one in.
        // A single cubic Bezier is defined by 4 points:
        //   [0] = start, [1] = first control, [2] = second control, [3] = end.
        // All four are drawn from xDist/yDist so they stay inside the client
        // area. The bezier pen's color is chosen from a small fixed palette
        // so curves stand out cleanly instead of disappearing into the busy
        // random-color shape field below them.
        if (use_beziers) {
          const COLORREF bezColor = customPalette[g_monochrome ? monoDist(rng) : paletteDist(rng)];
          HPEN hBezierPen = CreatePen(PS_SOLID, 1, bezColor);
          SelectObject(g_hdcMem, hBezierPen);
          const POINT pointArray[4] = {
            { xDist(rng), yDist(rng) },
            { xDist(rng), yDist(rng) },
            { xDist(rng), yDist(rng) },
            { xDist(rng), yDist(rng) },
          };
          PolyBezier(g_hdcMem, pointArray, static_cast<DWORD>(4));
          // Put the outline pen back so the cleanup below can delete the
          // bezier pen safely (a pen currently selected into a DC must not be
          // passed to DeleteObject).
          SelectObject(g_hdcMem, hPen);
          DeleteObject(hBezierPen);
        }
        // Draw a random straight line on top, same 1-in-6 gating as beziers.
        // Lines need just a start and end point. MoveToEx updates the DC's
        // current pen position (the last nullptr means we don't care about the
        // previous position); LineTo draws from there to (x1, y1) using the
        // currently selected pen.
        // Lines are constrained to horizontal, vertical, or the two 45° diagonals.
        // For the diagonals, a 45° angle means |dx| == |dy|, so we reuse the
        // same signed delta for both axes (sign flipped for the "/" direction).
        if (use_lines) {
          const COLORREF lineColor = customPalette[g_monochrome ? monoDist(rng) : paletteDist(rng)];
          HPEN hLinesPen = CreatePen(PS_SOLID, 1, lineColor);
          SelectObject(g_hdcMem, hLinesPen);
          const int x0 = xDist(rng);
          const int y0 = yDist(rng);
          int x1 = x0;
          int y1 = y0;
          switch (dirDist(rng)) {
            case 0: // horizontal: y stays, x moves
              x1 = xDist(rng);
              break;
            case 1: // vertical: x stays, y moves
              y1 = yDist(rng);
              break;
            case 2: { // diagonal "\" — dy == dx
              const int d = xDist(rng) - x0;
              x1 = x0 + d;
              y1 = y0 + d;
              break;
            }
            case 3: { // diagonal "/" — dy == -dx
              const int d = xDist(rng) - x0;
              x1 = x0 + d;
              y1 = y0 - d;
              break;
            }
            default:
              std::wcerr << __FUNC__ << L"distribution out of range!" << std::endl;
              break;
          }
          MoveToEx(g_hdcMem, x0, y0, nullptr);
          LineTo(g_hdcMem, x1, y1);
          SelectObject(g_hdcMem, hPen);
          DeleteObject(hLinesPen);
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
  // Fill the fresh bitmap with the current background color so newly exposed
  // areas on resize match the rest of the canvas.
  RECT rc = { 0, 0, cx, cy };
  HBRUSH hBrush = CreateSolidBrush(g_bkg_color);
  FillRect(g_hdcMem, &rc, hBrush);
  DeleteObject(hBrush);
  LeaveCriticalSection(&g_paintCS);
}

// Rewrites every pixel in the back buffer that currently equals oldColor so
// it becomes newColor. Shape pixels are left alone because their RGB values
// don't match the old background. Uses GetDIBits/SetDIBits to pull the bitmap
// into a CPU buffer, swap pixels in a tight loop, then push back.
//
// COLORREF is stored as 0x00BBGGRR (little-endian DWORD). A 32-bit BI_RGB DIB
// stores each pixel as BGRA in memory, which reads as 0xAARRGGBB as a DWORD.
// R and B are swapped between the two representations, so we build the
// comparison/replacement DWORDs explicitly rather than comparing COLORREFs.
void RecolorBackground(COLORREF oldColor, COLORREF newColor) {
  if (oldColor == newColor) return;

  EnterCriticalSection(&g_paintCS);
  if (g_hdcMem == nullptr || g_hbmMem == nullptr ||
      cxClient <= 0 || cyClient <= 0) {
    LeaveCriticalSection(&g_paintCS);
    return;
  }

  const int width  = cxClient;
  const int height = cyClient;

  BITMAPINFOHEADER bi = {};
  bi.biSize        = sizeof(BITMAPINFOHEADER);
  bi.biWidth       = width;
  bi.biHeight      = -height; // negative = top-down (simpler indexing)
  bi.biPlanes      = 1;
  bi.biBitCount    = 32;
  bi.biCompression = BI_RGB;

  std::vector<DWORD> pixels(static_cast<size_t>(width) * height);
  GetDIBits(g_hdcMem, g_hbmMem, 0, height, pixels.data(),
            reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);

  // Convert the two COLORREFs to the DIB's DWORD representation.
  const DWORD oldPix = (GetRValue(oldColor) << 16) |
                       (GetGValue(oldColor) << 8)  |
                        GetBValue(oldColor);
  const DWORD newPix = (GetRValue(newColor) << 16) |
                       (GetGValue(newColor) << 8)  |
                        GetBValue(newColor);

  // Mask off the high (reserved/alpha) byte when comparing so any noise there
  // doesn't cause false negatives on pixels that should match.
  for (auto& p : pixels) {
    if ((p & 0x00FFFFFF) == oldPix) {
      p = (p & 0xFF000000) | newPix;
    }
  }

  SetDIBits(g_hdcMem, g_hbmMem, 0, height, pixels.data(),
            reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);

  LeaveCriticalSection(&g_paintCS);
}

void SetNumShapes(const unsigned int num) {
  if (num > 8) {
    g_num_shapes = 8; // Cap at eight concurrent shapes.
  } else if (num == 0) {
    g_num_shapes = 1; // Handle invalid 0
  } else {
    g_num_shapes = num;
  }
}

bool ShowArt() {
  if (g_num_shapes == 0 || g_delay == 0) {
    std::wcerr << L"Number of shapes or delay Out of bounds!";
    return false;
  }

  // Auto-reset event: WaitForSingleObject in the art thread resets it
  // automatically, so the thread blocks again after each wakeup without
  // needing an explicit ResetEvent call.
  g_hDrawEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (g_hDrawEvent == nullptr) return false;

  g_running = true;
  HANDLE art_thread = CreateThread(nullptr, 0, ArtThread, nullptr, 0, nullptr);
  if (art_thread == nullptr) {
    g_running = false;
    CloseHandle(g_hDrawEvent);
    g_hDrawEvent = nullptr;
    return false;
  }
  CloseHandle(art_thread);

  // Start the timer that drives drawing. WM_TIMER fires every g_delay ms and
  // signals g_hDrawEvent to wake the art thread.
  if (!SetTimer(mainHwnd, TIMER_ART, g_delay, nullptr)) {
    g_running = false;
    SetEvent(g_hDrawEvent); // unblock thread so it can exit
    CloseHandle(g_hDrawEvent);
    g_hDrawEvent = nullptr;
    return false;
  }
  return true;
}

void TogglePaintArt(HWND hWnd) {
  if (hWnd == nullptr) {
    return;
  }
  g_paused = !g_paused;
  // Rather than tearing down the art thread, just stop (or restart) the timer
  // that drives it. While paused, no WM_TIMER messages fire, so g_hDrawEvent
  // is never signalled and the thread sits parked on WaitForSingleObject with
  // no CPU cost. The back buffer keeps its current contents untouched, which
  // is exactly what we want for Save As to capture.
  if (g_paused) {
    KillTimer(hWnd, TIMER_ART);
  } else {
    if (g_hDrawEvent != nullptr) {
      SetEvent(g_hDrawEvent); // Draw once immediately when resuming
    }
    SetTimer(hWnd, TIMER_ART, g_delay, nullptr);
  }
}
