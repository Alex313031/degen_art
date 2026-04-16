#ifndef DEGENART_UTILS_H_
#define DEGENART_UTILS_H_

#include "framework.h"

// Color constants
#define RGB_BLACK   RGB(0, 0, 0)
#define RGB_WHITE   RGB(255, 255, 255)
#define RGB_GREY    RGB(128, 128, 128)
#define RGB_RED     RGB(255, 0, 0)
#define RGB_GREEN   RGB(0, 255, 0)
#define RGB_BLUE    RGB(0, 0, 255)
#define RGB_YELLOW  RGB(255, 255, 0)
#define RGB_CYAN    RGB(0, 255, 255)
#define RGB_MAGENTA RGB(255, 0, 255)

// Time constants
inline constexpr unsigned long kSlowSpeed  = 2000UL;
inline constexpr unsigned long kMedSpeed   = 1000UL;
inline constexpr unsigned long kHighSpeed  = 500UL;
inline constexpr unsigned long kHyperSpeed = 250UL;

inline constexpr INT MINWIDTH  = 192;
inline constexpr INT MINHEIGHT = 192;

// Save client area as a .BMP photo, capturing moment menu was clicked.
bool SaveClientBitmap(HWND hWnd);

#endif // DEGENART_UTILS_H_
