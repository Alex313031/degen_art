#ifndef DEGENART_UTILS_H_
#define DEGENART_UTILS_H_

#include "framework.h"

#include "debugbreak.h" // Portable trap functions

inline const std::wstring sound_file = L"watersky.wav"; // Sound to play

extern volatile bool g_playsound;

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

// Chose which way to kill the app
enum TrapType {
  DEBUGBREAK = 0,
  ASSEMBLY   = 1,
  EXCEPTION  = 2,
  DIV0       = 3
};

// Time constants
inline constexpr unsigned long kSlowSpeed  = 2000UL;
inline constexpr unsigned long kMedSpeed   = 1000UL;
inline constexpr unsigned long kHighSpeed  = 500UL;
inline constexpr unsigned long kHyperSpeed = 250UL;

inline constexpr INT CW_WIDTH  = 640;
inline constexpr INT CW_HEIGHT = 640;

inline constexpr INT MINWIDTH  = 192;
inline constexpr INT MINHEIGHT = 192;

// Gets default settings from CHECKED state of menu items
void InitMenuDefaults(HWND hWnd);

// Gets the current side by side directory, regardless of where .exe is started from
const std::wstring GetExeDir();

// Save client area as a .BMP photo, capturing moment menu was clicked.
bool SaveClientBitmap(HWND hWnd);

const int TestTrap(TrapType type);

// Plays a .wav file (has to be side by side with the main .exe)
bool PlayWavFile(const std::wstring& wav_file);

// Stops playing any sound files currently playing
bool StopPlayWav();

// Starts and stops playing sound at will.
bool ToggleSound();

// Common controls color picker helper
bool PickColor(HWND hWnd);

#endif // DEGENART_UTILS_H_
