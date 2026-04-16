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

// Back buffer for preserving painted shapes
extern HDC g_hdcMem;
extern CRITICAL_SECTION g_paintCS;

#endif // DEGENART_GLOBALS_H_
