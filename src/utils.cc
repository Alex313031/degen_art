#include "utils.h"

#include <vector>

#include "art.h"
#include "globals.h"
#include "resource.h"

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
