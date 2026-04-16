#ifndef DEGENART_UTILS_H_
#define DEGENART_UTILS_H_

#include "framework.h"

// Color constants
#define RGB_BLACK   RGB(0, 0, 0)
#define RGB_WHITE   RGB(255, 255, 255)
#define RGB_RED     RGB(255, 0, 0)
#define RGB_GREEN   RGB(0, 255, 0)
#define RGB_BLUE    RGB(0, 0, 255)
#define RGB_YELLOW  RGB(255, 255, 0)
#define RGB_CYAN    RGB(0, 255, 255)
#define RGB_MAGENTA RGB(255, 0, 255)

// Save client area as a .BMP photo, capturing moment menu was clicked.
bool SaveClientBitmap(HWND hWnd);

#endif // DEGENART_UTILS_H_
