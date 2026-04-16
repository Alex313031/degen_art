#ifndef DEGENART_ART_H_
#define DEGENART_ART_H_

#include "framework.h"

extern bool g_circles;
extern bool g_both;
extern bool g_monochrome;
extern UINT g_num_shapes;
extern unsigned long g_delay;

// Back buffer for preserving painted shapes
extern HDC g_hdcMem;

// Bitmap buffer
extern HBITMAP g_hbmMem;

// Draws art shapes on a background thread, woken by g_hDrawEvent each timer tick.
DWORD WINAPI ArtThread(LPVOID pvoid);

// For handling back buffer bitmap for smooth resize
void RecreateBackBuffer(HWND hWnd, int cx, int cy);

#endif // DEGENART_ART_H_
