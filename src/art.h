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

// Hard upper bound on concurrent art threads. 8 matches the historical
// Windows 2000 Server / Windows XP CPU-license limit — beyond that on a
// weak 1-core box the drawing threads would just thrash the scheduler.
// The IDM_CONC_* menu currently only exposes 1..4, so this leaves room
// to expose up to 8 in the RC later without touching the thread-pool code.
#define kMaxArtThreads 8

// One art thread. Each thread waits on its own private auto-reset event and
// draws ONE shape per tick. The art-thread pool can be grown or shrunk at
// runtime (see EnsureThreadCount) so we never have more threads alive than
// the user asked for via the Concurrent Shapes menu.
DWORD WINAPI ArtThread(LPVOID pvoid);

// Resizes the art-thread pool to exactly `targetCount` live threads, spawning
// new ones or terminating excess ones as needed. Clamps to [1, kMaxArtThreads].
// Called internally from ShowArt and SetNumShapes; safe to call repeatedly.
bool EnsureThreadCount(int targetCount);

// Releases one tick to every currently-active art thread (SetEvent on each
// slot's private event). Call from WM_TIMER and wherever else a one-shot
// draw pulse is wanted (e.g. IDM_SINGLE, resume-from-pause).
void SignalArtTick();

// Terminates all art threads and closes their events/the timer. Called from
// WM_DESTROY; safe to call more than once.
void ShutdownArt();

// For handling back buffer bitmap for smooth resize
void RecreateBackBuffer(HWND hWnd, int cx, int cy);

// Swaps every pixel in the back buffer that currently equals oldColor over to
// newColor, leaving all other (shape) pixels untouched. Used by the background
// colour menu so the bg can change without erasing the art already painted.
void RecolorBackground(COLORREF oldColor, COLORREF newColor);

void SetNumShapes(const unsigned int num);

// Starts filling client area with abstract art
bool ShowArt();

// Pauses/resumes painting art, for i.e. taking a snapshot, or showing a friend the current state.
void TogglePaintArt(HWND hWnd);

#endif // DEGENART_ART_H_
