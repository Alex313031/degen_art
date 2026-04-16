#ifndef DEGENART_GLOBALS_H_
#define DEGENART_GLOBALS_H_

#include "framework.h"

// Main client width/height
extern int cxClient;
extern int cyClient;

// Our main window handle
extern HWND mainHwnd;

// Controlling art painting state
extern volatile bool g_running;

// For thread sync in painting
extern CRITICAL_SECTION g_paintCS;

// Current background color, changed via the Background Color menu
extern COLORREF g_bkg_color;

#endif // DEGENART_GLOBALS_H_
