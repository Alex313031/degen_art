#ifndef DEGENART_ART_H_
#define DEGENART_ART_H_

#include "framework.h"

extern volatile bool g_circles;
extern volatile bool g_beziers;
extern volatile bool g_lines;
extern volatile bool g_both;

extern bool g_monochrome;

extern volatile UINT g_num_shapes;

extern unsigned long g_delay;

// Back buffer for preserving painted shapes
extern HDC g_hdcMem;

// Bitmap buffer
extern HBITMAP g_hbmMem;

// Draws art shapes on a background thread, woken by g_hDrawEvent each timer tick.
DWORD WINAPI ArtThread(LPVOID pvoid);

// For handling back buffer bitmap for smooth resize
void RecreateBackBuffer(HWND hWnd, int cx, int cy);

// Swaps every pixel in the back buffer that currently equals oldColor over to
// newColor, leaving all other (shape) pixels untouched. Used by the background
// colour menu so the bg can change without erasing the art already painted.
void RecolorBackground(COLORREF oldColor, COLORREF newColor);

void SetNumShapes(const unsigned int num);

// Starts filling client area with abstract art
bool ShowArt();

// Pauses art, for i.e. taking a snapshot, or showing a friend the current state.
void PauseArt(HWND hWnd);

#endif // DEGENART_ART_H_
