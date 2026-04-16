#ifndef DEGENART_ART_H_
#define DEGENART_ART_H_

#include "framework.h"

extern bool g_circles;
extern UINT g_num_shapes;
extern unsigned long g_delay;

// Back buffer for preserving painted shapes
extern HDC g_hdcMem;

// Bitmap buffer
extern HBITMAP g_hbmMem;

// Draws da pretty art stuffz
DWORD WINAPI ArtThread(LPVOID pvoid);

// For handling back buffer bitmap for smooth resize
void RecreateBackBuffer(HWND hWnd, int cx, int cy);

#endif // DEGENART_ART_H_
