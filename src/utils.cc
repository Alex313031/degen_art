#include "utils.h"

#include <vector>

#include "art.h"
#include "globals.h"
#include "resource.h"

volatile bool g_playsound = false;

// Reads the CHECKED state of every menu group at startup and sets the
// corresponding globals. This makes all defaults entirely RC-driven: changing
// which item has CHECKED in degen_art.rc is the only code change needed to
// alter a default setting.
void InitMenuDefaults(HWND hWnd) {
  HMENU hMenu     = GetMenu(hWnd);
  HMENU hSettings = GetSubMenu(hMenu, 1);
  HMENU hShapes   = GetSubMenu(hSettings, 3);
  HMENU hBkgMenu  = GetSubMenu(hSettings, 6);
  HMENU hDelay    = GetSubMenu(hSettings, 8);

  // Shape mode — exactly one of the three items must be CHECKED in the RC
  if (GetMenuState(hShapes, IDM_RECTANGLES, MF_BYCOMMAND) & MF_CHECKED) {
    g_circles = false; g_both = false;
  } else if (GetMenuState(hShapes, IDM_ELLIPSES, MF_BYCOMMAND) & MF_CHECKED) {
    g_circles = true;  g_both = false;
  } else {
    g_circles = false; g_both = true; // IDM_BOTH
  }

  // Background color
  const struct { UINT id; COLORREF color; } bkgs[] = {
    { IDM_WHITE_BKG, RGB_WHITE },
    { IDM_BLACK_BKG, RGB_BLACK },
    { IDM_GREY_BKG,  RGB_GREY },
    { IDM_RED_BKG,   RGB_RED   },
    { IDM_GREEN_BKG, RGB_GREEN },
    { IDM_BLUE_BKG,  RGB_BLUE  },
  };
  for (const auto& b : bkgs) {
    if (GetMenuState(hBkgMenu, b.id, MF_BYCOMMAND) & MF_CHECKED) {
      g_bkg_color = b.color;
      break;
    }
  }

  // Draw delay
  const struct { UINT id; unsigned long ms; } delays[] = {
    { IDM_SLOW,   kSlowSpeed },
    { IDM_MEDIUM, kMedSpeed },
    { IDM_FAST,   kHighSpeed },
    { IDM_HYPER,  kHyperSpeed },
  };
  for (const auto& d : delays) {
    if (GetMenuState(hDelay, d.id, MF_BYCOMMAND) & MF_CHECKED) {
      g_delay = d.ms;
      break;
    }
  }

  // Monochrome toggle — grey out color bg items if CHECKED in the RC
  if (GetMenuState(hSettings, IDM_MONOCHROME, MF_BYCOMMAND) & MF_CHECKED) {
    g_monochrome = true;
    EnableMenuItem(hBkgMenu, IDM_RED_BKG,   MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(hBkgMenu, IDM_GREEN_BKG, MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(hBkgMenu, IDM_BLUE_BKG,  MF_BYCOMMAND | MF_GRAYED);
  }
}

const std::wstring GetExeDir() {
  wchar_t exe_path[MAX_PATH];
  HMODULE this_app = GetModuleHandleW(nullptr);
  if (!this_app) {
    return std::wstring();
  }
  DWORD got_path = GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
  if (got_path == 0 || got_path >= MAX_PATH) {
    return std::wstring();
  }

  // Find the last backslash to get the directory
  std::wstring fullPath(exe_path);
  size_t lastSlash = fullPath.find_last_of(L"\\/");
  std::wstring retval;
  if (lastSlash != std::wstring::npos) {
    retval = fullPath.substr(0, lastSlash + 1); // Include trailing slash
  } else {
    retval = fullPath;
  }
  return retval;
}

// Opens a system Save As dialog and writes the current back buffer to a 32-bit
// BMP file at the path the user chose.
//
// BMP layout (no palette for 32-bit):
//   BITMAPFILEHEADER  (14 bytes) — magic 'BM', file size, pixel data offset
//   BITMAPINFOHEADER  (40 bytes) — dimensions, bit depth, compression
//   Pixel data        (w * h * 4 bytes) — 32-bit BGRA, bottom-up row order
bool SaveClientBitmap(HWND hWnd) {
  // Prompt the user for a destination path
  wchar_t szFile[MAX_PATH]  = {};
  OPENFILENAMEW ofn         = {};
  ofn.lStructSize  = sizeof(OPENFILENAMEW);
  ofn.hwndOwner    = hWnd;
  ofn.lpstrFile    = szFile;
  ofn.nMaxFile     = MAX_PATH;
  ofn.lpstrFilter  = L"Bitmap Files (*.bmp)\0*.bmp\0All Files (*.*)\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.lpstrDefExt  = L"bmp";
  ofn.lpstrTitle   = L"Save Bitmap As";
  ofn.Flags        = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

  if (!GetSaveFileNameW(&ofn)) {
    return false; // user cancelled or dialog error
  }

  // Hold the back buffer lock for the duration of the pixel read so the art
  // thread cannot modify the bitmap mid-capture.
  EnterCriticalSection(&g_paintCS);

  if (g_hdcMem == nullptr || g_hbmMem == nullptr) {
    LeaveCriticalSection(&g_paintCS);
    return false;
  }

  // Query the actual bitmap dimensions from the GDI object
  BITMAP bm = {};
  GetObject(g_hbmMem, sizeof(BITMAP), &bm);
  const int width  = bm.bmWidth;
  const int height = bm.bmHeight;

  if (width <= 0 || height <= 0) {
    LeaveCriticalSection(&g_paintCS);
    return false;
  }

  // Describe the desired output: 32-bit bottom-up RGB (the standard BMP layout)
  // biHeight positive = bottom-up, which is what all BMP readers expect.
  BITMAPINFOHEADER bi = {};
  bi.biSize        = sizeof(BITMAPINFOHEADER);
  bi.biWidth       = width;
  bi.biHeight      = height;
  bi.biPlanes      = 1;
  bi.biBitCount    = 32;
  bi.biCompression = BI_RGB;
  bi.biSizeImage   = static_cast<DWORD>(width * height * 4);

  // GetDIBits copies the selected bitmap's pixels into our buffer in the format
  // described by bi. With BI_RGB and 32 bits, each pixel is 4 bytes: BGRA
  // (GDI leaves the alpha byte as 0, which is fine for BMP).
  std::vector<BYTE> pixels(bi.biSizeImage);
  GetDIBits(g_hdcMem, g_hbmMem, 0, static_cast<UINT>(height), pixels.data(),
            reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);

  LeaveCriticalSection(&g_paintCS);

  // Build the BMP file header
  const DWORD pixelDataOffset = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  BITMAPFILEHEADER bf = {};
  bf.bfType    = 0x4D42; // 'BM' signature
  bf.bfSize    = pixelDataOffset + bi.biSizeImage;
  bf.bfOffBits = pixelDataOffset;

  // Write the three sections to the file
  HANDLE hFile = CreateFileW(szFile, GENERIC_WRITE, 0, nullptr,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hFile == INVALID_HANDLE_VALUE) {
    return false;
  }

  DWORD written;
  const bool ok =
      WriteFile(hFile, &bf,           sizeof(bf),      &written, nullptr) &&
      WriteFile(hFile, &bi,           sizeof(bi),      &written, nullptr) &&
      WriteFile(hFile, pixels.data(), bi.biSizeImage,  &written, nullptr);

  CloseHandle(hFile);
  return ok;
}

inline static void __KillAssembly() {
#ifdef __MINGW32__
  asm("int3\n\t"
      "ud2");
#else
  __asm int 3 // Execute int3 interrupt
  __asm {
    UD2
  } // Execute 0x0F, 0x0B
#endif // __MINGW32__
}

const int TestTrap(TrapType type) {
  int retval;
  switch (type) {
    case DEBUGBREAK:
      __debugbreak();
      retval = 0;
      break;
    case ASSEMBLY:
      __KillAssembly();
      retval = 0;
      break;
    case EXCEPTION:
      debug_break();
      retval = 0;
      break;
    case DIV0:
      retval = 0 / 0;
      break;
    default:
      std::wcout << L"Invalid trap type! " << std::endl;
      retval = -1;
      break;
  }
  return retval;
}

bool PlayWavFile(const std::wstring& wav_file) {
  const std::wstring cwd = GetExeDir();
  if (wav_file.empty() || cwd.empty()) {
    return false;
  }
  static const DWORD playFlags = SND_FILENAME | SND_ASYNC | SND_LOOP;
  const std::wstring file = cwd + wav_file;
  const bool success = PlaySoundW(sound_file.c_str(), nullptr, playFlags);
  if (success) {
    g_playsound = true;
  } else {
    std::wcerr << L"Failed to play sound " << wav_file << std::endl;
    g_playsound = false;
  }
  return success;
}

bool StopPlayWav() {
  const bool success = PlaySoundW(nullptr, nullptr, 0);
  if (success) {
    g_playsound = false;
  } else {
    MessageBoxW(mainHwnd, L"Failed to stop playing sound!", L"PlaySound Error", MB_OK | MB_ICONERROR);
  }
  return success;
}

bool ToggleSound() {
  if (g_playsound) {
    return StopPlayWav();
  } else {
    return PlayWavFile(sound_file);
  }  
}
