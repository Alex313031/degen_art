#ifndef DEGENART_UTILS_H_
#define DEGENART_UTILS_H_

#include "framework.h"

#include <logging.h>

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

extern int g_toolbarHeight; // Height of the top toolbar in pixels; 0 if none. Art canvas lives below it.

extern COLORREF g_draw_color; // Pen color used while in draw mode; set by IDM_PICKCOLOR

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

// Helper functions for MessageBoxW
bool InfoBox(HWND hWnd, const std::wstring& title, const std::wstring& message);

bool WarnBox(HWND hWnd, const std::wstring& title, const std::wstring& message);

bool ErrorBox(HWND hWnd, const std::wstring& title, const std::wstring& message);

// Creates the app's top toolbar as a child of hParent. Call once from
// WM_CREATE. Stores the toolbar handle internally and measures the global
// g_toolbarHeight so the rest of the app can offset the art canvas below it.
void CreateAppToolbar(HWND hParent, HINSTANCE hInst);

// Re-auto-sizes the toolbar to the parent's new width and re-measures its
// height into g_toolbarHeight. Call from WM_SIZE. No-op if the toolbar
// hasn't been created yet.
void LayoutToolbar(HWND hWnd);

// Swaps the IDM_PAUSED toolbar button's icon+label between "Pause" (paint
// running) and "Play" (paint paused). Call after toggling g_paused.
void SetPauseButton(bool paused);

// Swaps the IDM_SOUND toolbar button's icon+label between "Music On" (silent,
// click to start) and "Mute" (playing, click to stop). Call after ToggleSound.
void SetSoundButton(bool playing);

// Swaps the IDM_DRAW toolbar button's icon+label between "Draw" (idle) and
// "Stop Draw" (draw mode active). Call after toggling g_draw_mode.
void SetDrawButton(bool drawing);

#endif // DEGENART_UTILS_H_
