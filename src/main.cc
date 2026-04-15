/*------------------------------------------
   Displays Random Rectangles, etc
   (c) Charles Petzold, 1998
   (c) Alex313031, 2026
  ------------------------------------------*/

#include "main.h"

#include "resource.h"
#include "version.h"

HWND mainHwnd = nullptr;

static HINSTANCE g_hInstance = nullptr;

int cxClient = 0;
int cyClient = 0;

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
  wndclass.style         = CS_HREDRAW | CS_VREDRAW ;
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

  static const LPCWSTR appTitle = APP_NAME;
  mainHwnd = CreateWindowExW(WS_EX_OVERLAPPEDWINDOW, szClassName, appTitle,
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
  return msg.wParam ;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_CREATE:
      InitApp(hWnd);
      break;
    case WM_SIZE:
      cxClient = LOWORD(lParam);
      cyClient = HIWORD(lParam);
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
        default:
          return DefWindowProc(hWnd, message, wParam, lParam);
      }
    } break;
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
  const bool show_circles = false;
  const unsigned int concurrent_shapes = 1;
  const unsigned long draw_delay = 333UL;

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
