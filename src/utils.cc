#include "utils.h"

#include "globals.h"
#include "resource.h"

bool SaveClientBitmap(HWND hWnd) {
  return false; // TODO
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

void TestTrap(TrapType type) {
  int retval;
  switch (type) {
    case DEBUGBREAK:
      __debugbreak();
    case ASSEMBLY:
      __KillAssembly();
    case EXCEPTION:
     debug_break();
    case DIV0:
      retval = 0 / 0;
    default:
      std::wcout << L"Invalid trap type! " << std::endl;
  }
}
