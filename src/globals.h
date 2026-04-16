#ifndef DEGENART_GLOBALS_H_
#define DEGENART_GLOBALS_H_

#include "framework.h"

// Main client width/height
extern int cxClient;
extern int cyClient;

// Our main window handle
extern HWND mainHwnd;

// Controlling art thread state
extern volatile bool g_running;

// For thread sync on back buffer access
extern CRITICAL_SECTION g_paintCS;

// Signalled by WM_TIMER each tick to wake the art thread
extern HANDLE g_hDrawEvent;

// Current background color, changed via the Background Color menu
extern COLORREF g_bkg_color;

#endif // DEGENART_GLOBALS_H_
