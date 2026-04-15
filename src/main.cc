/*------------------------------------------
   RNDRCTMT.C -- Displays Random Rectangles
                 (c) Charles Petzold, 1998
  ------------------------------------------*/

#include "main.h"

#include "resource.h"
#include "version.h"

HWND hwnd = nullptr;

int cxClient = 0;
int cyClient = 0;

int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPWSTR lpCmdLine,
                      int iCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  InitCommonControls();

  static const LPCWSTR szClassName = MAIN_WNDCLASS;
  MSG msg;
  WNDCLASSEXW wndclass;

  wndclass.cbSize        = sizeof(WNDCLASSEX);
  wndclass.style         = CS_HREDRAW | CS_VREDRAW ;
  wndclass.lpfnWndProc   = WindowProc;
  wndclass.cbClsExtra    = 0;
  wndclass.cbWndExtra    = 0;
  wndclass.hInstance     = hInstance;
  wndclass.hIcon         = LoadIcon(nullptr, MAKEINTRESOURCE(IDI_MAIN));
  wndclass.hCursor       = LoadCursor(nullptr, IDC_ARROW) ;
  wndclass.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
  wndclass.lpszMenuName  = MAKEINTRESOURCEW(IDC_MAIN);
  wndclass.lpszClassName = szClassName;
  wndclass.hIconSm       = LoadIcon(nullptr, MAKEINTRESOURCE(IDI_SMALL));

  if (!RegisterClassExW(&wndclass)) {
    MessageBox(nullptr, L"This program requires Windows NT!",
               szClassName, MB_ICONERROR) ;
    return 0;
  }

  static const LPCWSTR appTitle = APP_NAME;
  hwnd = CreateWindowExW(WS_EX_COMPOSITED, szClassName, appTitle,
                         WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_SIZEBOX,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         640, 480,
                         nullptr, nullptr, hInstance, nullptr);

  if (hwnd == nullptr) {
    return 1;
  }
  ShowWindow(hwnd, iCmdShow);
  UpdateWindow(hwnd) ;
     
  while (GetMessage (&msg, nullptr, 0, 0)) {
    TranslateMessage (&msg) ;
    DispatchMessage (&msg) ;
  }
  return msg.wParam ;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_CREATE:
      _beginthread(Thread, 0, nullptr);
      return 0;
    case WM_SIZE:
      cxClient = LOWORD(lParam);
      cyClient = HIWORD(lParam);
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProc(hwnd, message, wParam, lParam);
}

VOID Thread(PVOID pvoid) {
  HBRUSH hBrush;
  HDC hdc;
  int xLeft, xRight, yTop, yBottom;
  int iRed, iGreen, iBlue;

  while (TRUE) {
    if (cxClient != 0 || cyClient != 0) {
      xLeft   = rand() % cxClient;
      xRight  = rand() % cxClient;
      yTop    = rand() % cyClient;
      yBottom = rand() % cyClient;
      iRed    = rand() & 255;
      iGreen  = rand() & 255;
      iBlue   = rand() & 255;

      hdc = GetDC(hwnd);
      hBrush = CreateSolidBrush(RGB (iRed, iGreen, iBlue));
      SelectObject(hdc, hBrush);
      Rectangle(hdc, std::min(xLeft, xRight), std::min(yTop, yBottom),
                std::max(xLeft, xRight), std::max(yTop, yBottom));
      ReleaseDC(hwnd, hdc);
      DeleteObject(hBrush);
    }
	  Sleep(500);
  }
}
