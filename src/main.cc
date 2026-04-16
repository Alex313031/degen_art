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
  mainHwnd = CreateWindowExW(WS_EX_OVERLAPPEDWINDOW | WS_EX_COMPOSITED, szClassName, appTitle,
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
      InitApp(hWnd);
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
      // Fill the dirty region with white first. This covers two cases:
      //   1. The back buffer is not ready yet (startup).
      //   2. The window grew and ps.rcPaint extends beyond the back buffer's
      //      bounds — those new pixels would otherwise stay black.
      FillRect(hdc, &ps.rcPaint, reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
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
      g_running = false;
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
  DestroyWindow(mainHwnd);
}

bool InitApp(HWND hWnd) {
  if (hWnd == nullptr) {
    return false;
  }
  const bool show_circles = true;
  const unsigned int concurrent_shapes = 2;
  const unsigned long draw_delay = 500UL;

  if (!ShowArt(show_circles, concurrent_shapes, draw_delay)) {
    MessageBoxW(nullptr, L"ShowArt failed!", L"ShowArt Error", MB_OK | MB_ICONERROR);
    return false;
  }
  return true;
}

bool ShowArt(bool circles, unsigned int num_shapes, unsigned long delay) {
  if (num_shapes == 0 || delay == 0) {
    std::wcerr << L"Number of shapes or delay Out of bounds!";
    return false;
  }
  g_circles = circles;
  g_num_shapes = num_shapes;
  g_delay = delay;

  g_running = true;
  HANDLE art_thread = CreateThread(nullptr, 0, ArtThread, nullptr, 0, nullptr);
  if (art_thread == nullptr) {
    g_running = false;
    return false;
  }
  CloseHandle(art_thread);
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
