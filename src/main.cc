/*------------------------------------------
   Displays Random Rectangles, etc
   (c) Charles Petzold, 1998
   (c) Alex313031, 2026
  ------------------------------------------*/

#include "main.h"

#include "resource.h"
#include "utils.h"
#include "version.h"

HWND mainHwnd = nullptr;

static HINSTANCE g_hInstance = nullptr;

int cxClient = 0;
int cyClient = 0;

static bool s_resizing = false;
static POINT s_resizeOrigin = {};
static SIZE s_resizeStartSize = {};

HDC g_hdcMem     = nullptr;
HBITMAP g_hbmMem = nullptr;

// CRITICAL_SECTION is a lightweight Win32 synchronization primitive for mutual
// exclusion between threads on the same process. Unlike a mutex, it cannot be
// shared across processes, but is faster for intra-process use. We use it to
// prevent the art thread and the main thread from accessing the back buffer
// (g_hdcMem / g_hbmMem) at the same time.
CRITICAL_SECTION g_paintCS;

// Auto-reset event signalled by WM_TIMER each tick to wake the art thread.
// Auto-reset means it returns to non-signalled automatically after
// WaitForSingleObject unblocks, so the thread waits again next iteration.
HANDLE g_hDrawEvent = nullptr;

// Current background color. Defaults to white, changed via the Background
// Color menu. Used when filling the back buffer on resize and on WM_PAINT.
COLORREF g_bkg_color = RGB_WHITE;

// Reads the CHECKED state of every menu group at startup and sets the
// corresponding globals. This makes all defaults entirely RC-driven: changing
// which item has CHECKED in degen_art.rc is the only code change needed to
// alter a default setting.
static void InitMenuDefaults(HWND hWnd) {
  HMENU hMenu     = GetMenu(hWnd);
  HMENU hSettings = GetSubMenu(hMenu, 1);
  HMENU hShapes   = GetSubMenu(hSettings, 2);
  HMENU hBkgMenu  = GetSubMenu(hSettings, 3);
  HMENU hDelay    = GetSubMenu(hSettings, 4);

  // Shape mode — exactly one of the three items must be CHECKED in the RC
  if (GetMenuState(hShapes, IDM_RECTANGLES, MF_BYCOMMAND) & MF_CHECKED) {
    g_circles = false; g_both = false;
  } else if (GetMenuState(hShapes, IDM_ELLIPSES, MF_BYCOMMAND) & MF_CHECKED) {
    g_circles = true;  g_both = false;
  } else {
    g_circles = false; g_both = true; // IDM_BOTH
  }

  // Background color
  const struct { UINT id; COLORREF color; } bkgs[] = {
    { IDM_WHITE_BKG, RGB_WHITE },
    { IDM_BLACK_BKG, RGB_BLACK },
    { IDM_RED_BKG,   RGB_RED   },
    { IDM_GREEN_BKG, RGB_GREEN },
    { IDM_BLUE_BKG,  RGB_BLUE  },
  };
  for (const auto& b : bkgs) {
    if (GetMenuState(hBkgMenu, b.id, MF_BYCOMMAND) & MF_CHECKED) {
      g_bkg_color = b.color;
      break;
    }
  }

  // Draw delay
  const struct { UINT id; unsigned long ms; } delays[] = {
    { IDM_SLOW,   kSlowSpeed },
    { IDM_MEDIUM, kMedSpeed },
    { IDM_FAST,   kHighSpeed },
    { IDM_HYPER,  kHyperSpeed },
  };
  for (const auto& d : delays) {
    if (GetMenuState(hDelay, d.id, MF_BYCOMMAND) & MF_CHECKED) {
      g_delay = d.ms;
      break;
    }
  }

  // Monochrome toggle — grey out color bg items if CHECKED in the RC
  if (GetMenuState(hSettings, IDM_MONOCHROME, MF_BYCOMMAND) & MF_CHECKED) {
    g_monochrome = true;
    EnableMenuItem(hBkgMenu, IDM_RED_BKG,   MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(hBkgMenu, IDM_GREEN_BKG, MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(hBkgMenu, IDM_BLUE_BKG,  MF_BYCOMMAND | MF_GRAYED);
  }
}

int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPWSTR lpCmdLine,
                      int iCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  g_hInstance = hInstance;
  InitCommonControls();

  static const LPCWSTR szClassName = MAIN_WNDCLASS;

  WNDCLASSEXW wndclass;
  wndclass.cbSize        = sizeof(WNDCLASSEX);
  wndclass.style         = 0;
  wndclass.lpfnWndProc   = WindowProc;
  wndclass.cbClsExtra    = 0;
  wndclass.cbWndExtra    = 0;
  wndclass.hInstance     = hInstance;
  wndclass.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAIN));
  wndclass.hCursor       = LoadCursor(nullptr, IDC_ARROW) ;
  wndclass.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
  wndclass.lpszMenuName  = MAKEINTRESOURCEW(IDC_MAIN);
  wndclass.lpszClassName = szClassName;
  wndclass.hIconSm       = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SMALL));

  if (!RegisterClassExW(&wndclass)) {
    MessageBoxW(nullptr, L"This program requires Windows NT!",
               L"", MB_OK | MB_ICONERROR);
    return 2;
  }

  InitializeCriticalSection(&g_paintCS);

  static const LPCWSTR appTitle = APP_NAME;
  static const DWORD exStyle =
#if _WIN32_WINNT >= 0x0501
      WS_EX_OVERLAPPEDWINDOW | WS_EX_COMPOSITED;
#else
      WS_EX_OVERLAPPEDWINDOW;
#endif
  mainHwnd = CreateWindowExW(exStyle, szClassName, appTitle,
                         WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_SIZEBOX,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         640, 480,
                         nullptr, nullptr, hInstance, nullptr);

  if (mainHwnd == nullptr) {
    return 1;
  }
  ShowWindow(mainHwnd, iCmdShow);
  if (!UpdateWindow(mainHwnd)) {
    return 1;
  }

  HACCEL hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MAIN));

  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0)) {
    if (!TranslateAccelerator(mainHwnd, hAccel, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
  DeleteCriticalSection(&g_paintCS);
  return msg.wParam;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_CREATE:
      if (mainHwnd == nullptr) {
        mainHwnd = hWnd; // Prevent race condition in InitApp
      }
      // CreateCompatibleDC(nullptr) creates an off-screen memory DC compatible
      // with the screen. At this point it holds a 1x1 monochrome placeholder
      // bitmap; RecreateBackBuffer (called on the first WM_SIZE) replaces it
      // with a full-size bitmap matched to the window.
      g_hdcMem = CreateCompatibleDC(nullptr);
      InitMenuDefaults(hWnd);
      InitApp(hWnd);
      break;
    case WM_TIMER:
      // WM_TIMER fires on the main thread at the interval set by SetTimer.
      // We signal the art thread's event rather than drawing here directly,
      // keeping all GDI work on the art thread and leaving the main thread
      // free to process input and paint messages without stalling.
      if (wParam == TIMER_ART && g_hDrawEvent != nullptr) {
        SetEvent(g_hDrawEvent);
      }
      break;
    case WM_ERASEBKGND:
      // Returning TRUE tells Windows we have handled background erasing
      // ourselves, suppressing the default white fill. We do our own filling
      // in WM_PAINT so the two operations don't race or double-paint.
      return TRUE;
    case WM_GETMINMAXINFO: {
      // Set the minimum size for the window
      LPMINMAXINFO pMinMaxInfo      = reinterpret_cast<LPMINMAXINFO>(lParam);
      pMinMaxInfo->ptMinTrackSize.x = 106;
      pMinMaxInfo->ptMinTrackSize.y = 80;
      pMinMaxInfo->ptMaxTrackSize.x = 1920;
      pMinMaxInfo->ptMaxTrackSize.y = 1080;
      break;
    }
    case WM_PAINT: {
      // BeginPaint validates the dirty region and fills ps.rcPaint with the
      // bounding rect of the area Windows wants us to repaint. Nothing is
      // drawn to the screen until EndPaint is called.
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hWnd, &ps);
      // Fill the dirty region with the background color first. This covers two
      // cases:
      //   1. The back buffer is not ready yet (startup).
      //   2. The window grew and ps.rcPaint extends beyond the back buffer's
      //      bounds — those new pixels would otherwise stay black.
      HBRUSH hBkgBrush = CreateSolidBrush(g_bkg_color);
      FillRect(hdc, &ps.rcPaint, hBkgBrush);
      DeleteObject(hBkgBrush);
      // Hold the lock so the art thread cannot replace the back buffer
      // bitmap between our null-check and the BitBlt call.
      EnterCriticalSection(&g_paintCS);
      if (g_hdcMem != nullptr && g_hbmMem != nullptr) {
        // BitBlt copies only the invalidated rectangle from the back buffer
        // to the window DC. SRCCOPY means a straight pixel copy with no
        // blending. This restores any shapes the art thread has drawn there,
        // including regions uncovered by other windows being moved away.
        BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
               ps.rcPaint.right - ps.rcPaint.left,
               ps.rcPaint.bottom - ps.rcPaint.top,
               g_hdcMem, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
      }
      LeaveCriticalSection(&g_paintCS);
      EndPaint(hWnd, &ps);
      break;
    }
    case WM_SIZE:
      cxClient = LOWORD(lParam);
      cyClient = HIWORD(lParam);
      // The client area changed size, so recreate the back buffer to match.
      // If the window shrank, the old bitmap is too large and wastes memory.
      // If it grew, the old bitmap is too small and BitBlt would read outside
      // its bounds, producing black pixels in the newly exposed area.
      RecreateBackBuffer(hWnd, cxClient, cyClient);
      break;
    case WM_COMMAND: {
      const int command = LOWORD(wParam);
      switch (command) {
        case IDM_EXIT:
          ShutDownApp();
          break;
        case IDM_ABOUT:
          DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_ABOUTDLG), hWnd, AboutDlgProc);
          break;
        case IDM_HELP:
          LaunchHelp(hWnd);
          break;
        case IDM_SAVE_AS:
          SaveClientBitmap(hWnd);
          break;
        case IDM_MONOCHROME: {
          g_monochrome = !g_monochrome;
          HMENU hSettings = GetSubMenu(GetMenu(hWnd), 1);
          // Toggle the check mark on the menu item to show current state.
          CheckMenuItem(hSettings, IDM_MONOCHROME,
                        MF_BYCOMMAND | (g_monochrome ? MF_CHECKED : MF_UNCHECKED));
          // Grey out or restore the color background options — only white and
          // black are valid background choices in monochrome mode.
          HMENU hBkgMenu = GetSubMenu(hSettings, 3);
          const UINT colorState = g_monochrome ? MF_GRAYED : MF_ENABLED;
          EnableMenuItem(hBkgMenu, IDM_RED_BKG,   MF_BYCOMMAND | colorState);
          EnableMenuItem(hBkgMenu, IDM_GREEN_BKG, MF_BYCOMMAND | colorState);
          EnableMenuItem(hBkgMenu, IDM_BLUE_BKG,  MF_BYCOMMAND | colorState);
          // If switching into monochrome and the current background is a color
          // (not white or black), reset it to white.
          if (g_monochrome && g_bkg_color != RGB_WHITE && g_bkg_color != RGB_BLACK) {
            g_bkg_color = RGB_WHITE;
            CheckMenuRadioItem(hBkgMenu, IDM_WHITE_BKG, IDM_BLUE_BKG, IDM_WHITE_BKG, MF_BYCOMMAND);
          }
          // Always clear the back buffer on toggle so no mixed color/monochrome
          // shapes remain visible after the mode change.
          EnterCriticalSection(&g_paintCS);
          if (g_hdcMem != nullptr && g_hbmMem != nullptr) {
            RECT rc = { 0, 0, cxClient, cyClient };
            HBRUSH hBrush = CreateSolidBrush(g_bkg_color);
            FillRect(g_hdcMem, &rc, hBrush);
            DeleteObject(hBrush);
          }
          LeaveCriticalSection(&g_paintCS);
          InvalidateRect(hWnd, nullptr, FALSE);
          break;
        }
        case IDM_WHITE_BKG:
        case IDM_BLACK_BKG:
        case IDM_RED_BKG:
        case IDM_GREEN_BKG:
        case IDM_BLUE_BKG: {
          HMENU hSettings = GetSubMenu(GetMenu(hWnd), 1);
          HMENU hBkgMenu  = GetSubMenu(hSettings, 3);
          CheckMenuRadioItem(hBkgMenu, IDM_WHITE_BKG, IDM_BLUE_BKG, command, MF_BYCOMMAND);
          switch (command) {
            case IDM_WHITE_BKG: g_bkg_color = RGB_WHITE; break;
            case IDM_BLACK_BKG: g_bkg_color = RGB_BLACK; break;
            case IDM_RED_BKG:   g_bkg_color = RGB_RED;   break;
            case IDM_GREEN_BKG: g_bkg_color = RGB_GREEN; break;
            default:            g_bkg_color = RGB_BLUE;  break;
          }
          // Fill the back buffer with the new color immediately so the change
          // is visible right away rather than only on the next resize.
          EnterCriticalSection(&g_paintCS);
          if (g_hdcMem != nullptr && g_hbmMem != nullptr) {
            RECT rc = { 0, 0, cxClient, cyClient };
            HBRUSH hBrush = CreateSolidBrush(g_bkg_color);
            FillRect(g_hdcMem, &rc, hBrush);
            DeleteObject(hBrush);
          }
          LeaveCriticalSection(&g_paintCS);
          // Invalidate the whole client area so WM_PAINT blits the updated
          // back buffer to the screen. FALSE = do not erase background first
          // (we handle that in WM_PAINT ourselves).
          InvalidateRect(hWnd, nullptr, FALSE);
          break;
        }
        case IDM_SLOW:
        case IDM_MEDIUM:
        case IDM_FAST:
        case IDM_HYPER: {
          HMENU hSettings = GetSubMenu(GetMenu(hWnd), 1);
          HMENU hDelay    = GetSubMenu(hSettings, 4);
          CheckMenuRadioItem(hDelay, IDM_SLOW, IDM_HYPER, command, MF_BYCOMMAND);
          switch (command) {
            case IDM_SLOW:   g_delay = 2000UL; break;
            case IDM_MEDIUM: g_delay = 1000UL; break;
            case IDM_FAST:   g_delay =  500UL; break;
            default:         g_delay =  250UL; break; // IDM_HYPER
          }
          // Replace the timer with the new interval. SetTimer on an existing ID
          // replaces it in place — the next tick will be at the new rate.
          SetTimer(hWnd, TIMER_ART, g_delay, nullptr);
          // Immediately signal the draw event so the art thread stops waiting on
          // the old interval and re-blocks on the next WaitForSingleObject call,
          // which will then wake at the new rate. Without this the thread would
          // sit out the remainder of the previous tick before noticing the change.
          if (g_hDrawEvent != nullptr) SetEvent(g_hDrawEvent);
          break;
        }
        case IDM_RECTANGLES:
        case IDM_ELLIPSES:
        case IDM_BOTH: {
          // CheckMenuRadioItem places a radio-button style check mark on the
          // chosen item and clears the check from all others in the ID range.
          // MF_BYCOMMAND means the IDs are item command values, not positions.
          HMENU hSettings = GetSubMenu(GetMenu(hWnd), 1);
          HMENU hShapes   = GetSubMenu(hSettings, 2);
          CheckMenuRadioItem(hShapes, IDM_RECTANGLES, IDM_BOTH, command, MF_BYCOMMAND);
          g_both    = (command == IDM_BOTH);
          g_circles = (command == IDM_ELLIPSES);
          break;
        }
        default:
          return DefWindowProc(hWnd, message, wParam, lParam);
      }
    } break;
    case WM_LBUTTONDOWN:
      ReleaseCapture();
      SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
      break;
    case WM_RBUTTONDOWN: {
      RECT rc;
      GetCursorPos(&s_resizeOrigin);
      GetWindowRect(hWnd, &rc);
      s_resizeStartSize = { rc.right - rc.left, rc.bottom - rc.top };
      s_resizing = true;
      SetCapture(hWnd);
      break;
    }
    case WM_MOUSEMOVE: {
      if (s_resizing) {
        POINT pt;
        GetCursorPos(&pt);
        int w = s_resizeStartSize.cx + (pt.x - s_resizeOrigin.x);
        int h = s_resizeStartSize.cy + (pt.y - s_resizeOrigin.y);
        if (w < GetSystemMetrics(SM_CXMINTRACK)) w = GetSystemMetrics(SM_CXMINTRACK);
        if (h < GetSystemMetrics(SM_CYMINTRACK)) h = GetSystemMetrics(SM_CYMINTRACK);
        SetWindowPos(hWnd, nullptr, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
      }
      break;
    }
    case WM_RBUTTONUP:
      s_resizing = false;
      ReleaseCapture();
      break;
    case WM_CAPTURECHANGED:
      s_resizing = false;
      break;
    case WM_HELP:
      LaunchHelp(hWnd);
      break;
    case WM_CLOSE:
      ShutDownApp();
      break;
    case WM_QUERYENDSESSION:
      return TRUE;
    case WM_DESTROY:
      // Stop the timer first so no more WM_TIMER messages are queued.
      KillTimer(hWnd, TIMER_ART);
      // Signal the art thread to exit, then close the event handle.
      g_running = false;
      if (g_hDrawEvent != nullptr) {
        SetEvent(g_hDrawEvent);
        CloseHandle(g_hDrawEvent);
        g_hDrawEvent = nullptr;
      }
      // Clean up the back buffer. Order matters: DeleteDC first deselects
      // g_hbmMem from the memory DC, after which DeleteObject can safely free
      // the bitmap. Deleting a bitmap that is still selected into a DC is
      // undefined behavior in Win32.
      EnterCriticalSection(&g_paintCS);
      if (g_hdcMem != nullptr) {
        DeleteDC(g_hdcMem);
        g_hdcMem = nullptr;
      }
      if (g_hbmMem != nullptr) {
        DeleteObject(g_hbmMem);
        g_hbmMem = nullptr;
      }
      LeaveCriticalSection(&g_paintCS);
      PostQuitMessage(0);
      break;
    case WM_NCDESTROY:
      mainHwnd = nullptr;
      break;
    default:
      return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}

void ShutDownApp() {
  g_running = false;
  // Unblock the art thread so it can observe g_running=false and exit cleanly
  // rather than waiting indefinitely on g_hDrawEvent.
  if (g_hDrawEvent != nullptr) SetEvent(g_hDrawEvent);
  DestroyWindow(mainHwnd);
}

bool InitApp(HWND hWnd) {
  if (hWnd == nullptr) {
    return false;
  }
  // All settings (shape mode, delay, background color) are already set by
  // InitMenuDefaults. Only the fixed concurrent shape count is passed here.
  const unsigned int concurrent_shapes = 2;
  if (!ShowArt(concurrent_shapes)) {
    MessageBoxW(nullptr, L"ShowArt failed!", L"ShowArt Error", MB_OK | MB_ICONERROR);
    return false;
  }
  return true;
}

bool ShowArt(unsigned int num_shapes) {
  if (num_shapes == 0 || g_delay == 0) {
    std::wcerr << L"Number of shapes or delay Out of bounds!";
    return false;
  }
  g_num_shapes = num_shapes;

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

bool LaunchHelp(HWND hWnd) {
  if (MessageBoxW(hWnd, L"No help yet...", L"Help Error", MB_OK | MB_ICONWARNING) == IDOK) {
    return true;
  }
  return false;
}

INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
  UNREFERENCED_PARAMETER(lParam);
  switch (message) {
    case WM_INITDIALOG:
      return TRUE;
    case WM_COMMAND:
      if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
        EndDialog(hDlg, LOWORD(wParam));
        return TRUE;
      }
      break;
  }
  return FALSE;
}
