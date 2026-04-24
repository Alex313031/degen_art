#include "art.h"

#include "globals.h"
#include "resource.h"
#include "utils.h"

volatile bool g_running = false; // Global art threads running state
volatile bool g_paused  = false; // Affects g_running, used by IDM_PAUSED

// These are only placeholders — the authoritative defaults are read from the
// menu's CHECKED states by InitMenuDefaults (utils.cc) during WM_CREATE.
// See degen_art.rc for which items actually carry the CHECKED flag.
volatile bool g_circles = false;
volatile bool g_beziers = false;
volatile bool g_lines   = false;
volatile bool g_both    = true;

bool g_monochrome = false; // Whether monochrome colors only is enabled

volatile UINT g_num_shapes = 1; // Initialize to 1, in case something goes wrong at least we draw 1 shape

unsigned long g_delay = 500UL; // Default to same as .rc file (IDM_FAST)

// --- Thread pool state ----------------------------------------------------
// Each live art thread has its own auto-reset "tick" event and an exit flag.
// WM_TIMER (via SignalArtTick) calls SetEvent on exactly s_activeCount of
// these every tick, so each thread wakes once per tick and draws ONE shape.
// This keeps total shapes-per-tick == thread count (== g_num_shapes), and
// lets us dynamically spawn/terminate individual threads when the user
// changes the Concurrent Shapes setting.
struct ArtThreadSlot {
  HANDLE        hThread       = nullptr;
  HANDLE        hTickEvent    = nullptr; // auto-reset; SetEvent = "go draw"
  volatile bool exitRequested = false;   // set true to make thread exit cleanly
};
static ArtThreadSlot s_slots[kMaxArtThreads];
static int           s_activeCount = 0;  // only touched from the main thread

DWORD WINAPI ArtThread(LPVOID pvoid) {
  ArtThreadSlot* slot = static_cast<ArtThreadSlot*>(pvoid);
  if (mainHwnd == nullptr || slot == nullptr) {
    return 0x00000001;
  }
  // Every thread owns its own drawing scratch state and RNG / distributions.
  // Nothing here is shared, so none of it needs synchronization; the shared
  // state (g_hdcMem, the back buffer bitmap) is protected by g_paintCS below.
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
  std::uniform_int_distribution<int> diceDist(0, 5);     // 1-in-6 (beziers)
  std::uniform_int_distribution<int> lineDiceDist(0, 4); // 1-in-5 (lines)
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

  int xLeft = 0, xRight = 0, yTop = 0, yBottom = 0;
  int iRed  = 0, iGreen = 0, iBlue = 0;

  while (true) {
    // Block until SignalArtTick signals this slot's private event. Auto-reset,
    // so it returns to non-signalled immediately and we block again on the
    // next iteration. Any failure / spurious wake exits the thread.
    if (slot->hTickEvent == nullptr ||
        WaitForSingleObject(slot->hTickEvent, INFINITE) != WAIT_OBJECT_0) {
      break;
    }
    // Two exit paths: global shutdown OR this individual slot was asked to die
    // (EnsureThreadCount shrinking the pool).
    if (!g_running || slot->exitRequested) break;
    if (cxClient == 0 && cyClient == 0) {
      continue; // Window is minimized; wait for restore
    }

    // Serialize every GDI operation on the back buffer — multiple art threads
    // can be inside this section trying to enter at the same time, and the
    // main thread also grabs it in WM_PAINT and RecreateBackBuffer.
    EnterCriticalSection(&g_paintCS);
    if (g_hdcMem != nullptr) {
      std::uniform_int_distribution<int> xDist(0, cxClient - 1);
      std::uniform_int_distribution<int> yDist(0, cyClient - 1);

      // --- Draw exactly ONE shape ----------------------------------------
      xLeft   = xDist(rng);
      xRight  = xDist(rng);
      yTop    = yDist(rng);
      yBottom = yDist(rng);
      if (g_monochrome) {
        iRed = iGreen = iBlue = colorDist(rng);
      } else {
        iRed    = colorDist(rng);
        iGreen  = colorDist(rng);
        iBlue   = colorDist(rng);
      }
      COLORREF outlineColor;
      if (g_monochrome) {
        outlineColor = (iRed <= 128) ? RGB_WHITE : RGB_BLACK;
      } else {
        outlineColor = RGB(255 - iRed, 255 - iGreen, 255 - iBlue);
      }
      HPEN hPen = CreatePen(PS_SOLID, 1, outlineColor);
      hBrush    = CreateSolidBrush(RGB(iRed, iGreen, iBlue));
      SelectObject(g_hdcMem, hPen);
      SelectObject(g_hdcMem, hBrush);

      const bool use_ellipse = g_both ? (coinDist(rng) != 0) : g_circles;
      const bool use_beziers = g_beziers ? (diceDist(rng) == 0)     : false;
      const bool use_lines   = g_lines   ? (lineDiceDist(rng) == 0) : false;

      if (use_ellipse) {
        Ellipse(g_hdcMem, std::min(xLeft, xRight), std::min(yTop, yBottom),
                std::max(xLeft, xRight), std::max(yTop, yBottom));
      } else {
        Rectangle(g_hdcMem, std::min(xLeft, xRight), std::min(yTop, yBottom),
                  std::max(xLeft, xRight), std::max(yTop, yBottom));
      }

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
        SelectObject(g_hdcMem, hPen);
        DeleteObject(hBezierPen);
      }

      if (use_lines) {
        const COLORREF lineColor = customPalette[g_monochrome ? monoDist(rng) : paletteDist(rng)];
        HPEN hLinesPen = CreatePen(PS_SOLID, 1, lineColor);
        SelectObject(g_hdcMem, hLinesPen);
        const int x0 = xDist(rng);
        const int y0 = yDist(rng);
        int x1 = x0;
        int y1 = y0;
        switch (dirDist(rng)) {
          case 0: x1 = xDist(rng); break;                                 // horizontal
          case 1: y1 = yDist(rng); break;                                 // vertical
          case 2: { const int d = xDist(rng) - x0; x1 = x0 + d; y1 = y0 + d; break; } // "\"
          case 3: { const int d = xDist(rng) - x0; x1 = x0 + d; y1 = y0 - d; break; } // "/"
          default: LOG(ERROR) << L"distribution out of range!"; break;
        }
        MoveToEx(g_hdcMem, x0, y0, nullptr);
        LineTo(g_hdcMem, x1, y1);
        SelectObject(g_hdcMem, hPen);
        DeleteObject(hLinesPen);
      }

      SelectObject(g_hdcMem, hOldPen);
      DeleteObject(hPen);
      DeleteObject(hBrush);
      // --- /shape --------------------------------------------------------

      // Present the back buffer. With N threads each doing a BitBlt per tick
      // this is N blits instead of 1, but BitBlt is cheap and the result is
      // the same (each blit shows the current state of the accumulated back
      // buffer — later blits just reveal whatever later threads drew).
      hdc = GetDC(mainHwnd);
      BitBlt(hdc, 0, g_toolbarHeight, cxClient, cyClient, g_hdcMem, 0, 0, SRCCOPY);
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

// --- Thread pool management -----------------------------------------------
// These run on the main (UI) thread, never from inside ArtThread itself, so
// mutating s_slots / s_activeCount doesn't need its own critical section.

bool EnsureThreadCount(int targetCount) {
  if (targetCount < 1)              targetCount = 1;
  if (targetCount > kMaxArtThreads) targetCount = kMaxArtThreads;

  // Grow: spawn new slots up to targetCount.
  while (s_activeCount < targetCount) {
    const int i = s_activeCount;
    s_slots[i].exitRequested = false;
    s_slots[i].hTickEvent    = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (s_slots[i].hTickEvent == nullptr) return false;
    s_slots[i].hThread = CreateThread(nullptr, 0, ArtThread, &s_slots[i], 0, nullptr);
    if (s_slots[i].hThread == nullptr) {
      CloseHandle(s_slots[i].hTickEvent);
      s_slots[i].hTickEvent = nullptr;
      return false;
    }
    s_activeCount++;
  }

  // Shrink: ask the highest-indexed threads to exit, one by one. The thread
  // can only observe exitRequested after a wake, so we SetEvent to force it
  // to run the check. Then join and clean up.
  while (s_activeCount > targetCount) {
    const int i = s_activeCount - 1;
    s_slots[i].exitRequested = true;
    SetEvent(s_slots[i].hTickEvent);
    WaitForSingleObject(s_slots[i].hThread, INFINITE);
    CloseHandle(s_slots[i].hThread);
    CloseHandle(s_slots[i].hTickEvent);
    s_slots[i].hThread    = nullptr;
    s_slots[i].hTickEvent = nullptr;
    s_activeCount--;
  }
  return true;
}

void SignalArtTick() {
  // Release one tick to each currently-active thread. Auto-reset events mean
  // each SetEvent wakes exactly one waiter (the thread waiting on that specific
  // event), so all s_activeCount threads wake together per tick.
  for (int i = 0; i < s_activeCount; i++) {
    if (s_slots[i].hTickEvent != nullptr) {
      SetEvent(s_slots[i].hTickEvent);
    }
  }
}

void ShutdownArt() {
  g_running = false;
  // Wake every live thread so they can observe g_running=false and exit.
  for (int i = 0; i < s_activeCount; i++) {
    if (s_slots[i].hTickEvent != nullptr) {
      SetEvent(s_slots[i].hTickEvent);
    }
  }
  // Then join + close handles.
  for (int i = 0; i < s_activeCount; i++) {
    if (s_slots[i].hThread != nullptr) {
      WaitForSingleObject(s_slots[i].hThread, INFINITE);
      CloseHandle(s_slots[i].hThread);
      s_slots[i].hThread = nullptr;
    }
    if (s_slots[i].hTickEvent != nullptr) {
      CloseHandle(s_slots[i].hTickEvent);
      s_slots[i].hTickEvent = nullptr;
    }
  }
  s_activeCount = 0;
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
  // Fast-path: if the existing bitmap is already exactly the requested size,
  // leave it in place. This makes minimize → restore preserve the painted
  // canvas (WM_SIZE fires with the pre-minimize dims on restore, so cx/cy
  // match), and also avoids pointless bitmap churn on any other WM_SIZE that
  // doesn't actually change dims. GetObject(HBITMAP, ...) fills a BITMAP
  // struct with bmWidth/bmHeight for a DDB — no DC / lock required.
  if (g_hbmMem != nullptr) {
    BITMAP bm = {};
    if (GetObject(g_hbmMem, sizeof(bm), &bm) != 0 &&
        bm.bmWidth == cx && bm.bmHeight == cy) {
      return;
    }
  }
  // Slow path: dimensions changed, allocate a fresh bitmap. Borrow the
  // window DC only to query its pixel format for CreateCompatibleBitmap.
  HDC hdcWin = GetDC(hWnd);
  HBITMAP hbmNew = CreateCompatibleBitmap(hdcWin, cx, cy);
  ReleaseDC(hWnd, hdcWin);
  if (hbmNew == nullptr) return;

  // Hold the lock while swapping the bitmap so the art thread cannot draw into
  // g_hdcMem while we are replacing what it points at.
  EnterCriticalSection(&g_paintCS);
  // Prime hbmNew through a scratch DC: fill with bg, then blit the old
  // back buffer's contents into the top-left. This preserves existing
  // art across a resize — and also covers any minimize-then-restore
  // path where something fires an intermediate WM_SIZE and triggers
  // this slow branch. On grow, the extra margin stays bg; on shrink,
  // the excess rows / columns of the old bitmap get clipped off.
  HDC hdcScratch = CreateCompatibleDC(g_hdcMem);
  HBITMAP hbmScratchPrev = static_cast<HBITMAP>(SelectObject(hdcScratch, hbmNew));
  RECT rc = { 0, 0, cx, cy };
  HBRUSH hBrush = CreateSolidBrush(g_bkg_color);
  FillRect(hdcScratch, &rc, hBrush);
  DeleteObject(hBrush);
  if (g_hbmMem != nullptr) {
    BITMAP bmOld = {};
    if (GetObject(g_hbmMem, sizeof(bmOld), &bmOld) != 0) {
      const int copyW = (bmOld.bmWidth  < cx) ? bmOld.bmWidth  : cx;
      const int copyH = (bmOld.bmHeight < cy) ? bmOld.bmHeight : cy;
      BitBlt(hdcScratch, 0, 0, copyW, copyH, g_hdcMem, 0, 0, SRCCOPY);
    }
  }
  // Un-select hbmNew from the scratch DC so we can re-select it into g_hdcMem
  // (a bitmap can only be selected into one DC at a time).
  SelectObject(hdcScratch, hbmScratchPrev);
  DeleteDC(hdcScratch);

  // Promote hbmNew to be the live back buffer. SelectObject implicitly
  // deselects the previously-selected bitmap, which then becomes safe
  // to delete.
  SelectObject(g_hdcMem, hbmNew);
  if (g_hbmMem != nullptr) DeleteObject(g_hbmMem);
  g_hbmMem = hbmNew;
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
  unsigned int clamped = num;
  if (clamped > kMaxArtThreads) clamped = kMaxArtThreads;
  if (clamped == 0)             clamped = 1;
  g_num_shapes = clamped;
  // If the pool is already running (i.e. we're past ShowArt), resize it to
  // match. Before ShowArt there is nothing to resize — ShowArt will spawn
  // the right number of threads using g_num_shapes directly.
  if (g_running) {
    EnsureThreadCount(static_cast<int>(clamped));
  }
}

bool ShowArt() {
  if (g_num_shapes == 0 || g_delay == 0) {
    LOG(ERROR) << L"Number of shapes or delay Out of bounds!";
    return false;
  }

  // Spin up the initial thread pool matching the current Concurrent Shapes
  // setting. Each thread owns its own auto-reset wake event and blocks on
  // it until SignalArtTick (driven by WM_TIMER) says "go."
  g_running = true;
  if (!EnsureThreadCount(static_cast<int>(g_num_shapes))) {
    ShutdownArt();
    return false;
  }

  // Start the timer that drives drawing. WM_TIMER fires every g_delay ms
  // and, via SignalArtTick, pulses every active thread's tick event once.
  if (!SetTimer(mainHwnd, TIMER_ART, g_delay, nullptr)) {
    ShutdownArt();
    return false;
  }
  return true;
}

void TogglePaintArt(HWND hWnd) {
  if (hWnd == nullptr) {
    return;
  }
  g_paused = !g_paused;
  // Pause = kill the timer so no more ticks fire. Every active thread sits
  // parked on its tick event, zero CPU. Resume = re-arm the timer and also
  // give one immediate pulse so the window doesn't wait up to g_delay ms
  // before redrawing.
  //
  // Music tracks the paint-pause: when the canvas freezes we silence the
  // background track, when it thaws we pick up playback where we left off.
  // Every pause/resume (menu, toolbar, draw-mode auto-pause, single-step's
  // first press) funnels through here, so a single pair of calls covers
  // every entry point. Single-step's subsequent pulses go through
  // SignalArtTick directly, not TogglePaintArt, so audio correctly stays
  // silent across each step until the user actually resumes painting.
  if (g_paused) {
    KillTimer(hWnd, TIMER_ART);
    PauseMusicForPaint();
  } else {
    ResumeMusicForPaint();
    SignalArtTick();
    SetTimer(hWnd, TIMER_ART, g_delay, nullptr);
  }
}
