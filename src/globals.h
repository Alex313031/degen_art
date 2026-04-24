#ifndef DEGENART_GLOBALS_H_
#define DEGENART_GLOBALS_H_

#include "framework.h"

// Main client width/height
extern int cxClient;
extern int cyClient;

extern HWND mainHwnd; // Our main window handle

extern volatile bool g_running; // Controlling art thread state
extern volatile bool g_paused;  // Keep track of paused state. PauseArt() uses this in utils.cc

extern CRITICAL_SECTION g_paintCS; // For thread sync on back buffer access

extern COLORREF g_bkg_color; // Current background color, changed via the Background Color menu

extern bool g_draw_mode; // True when IDM_DRAW is active — left-click paints instead of moving window

#endif // DEGENART_GLOBALS_H_
