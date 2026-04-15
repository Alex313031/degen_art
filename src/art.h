#ifndef DEGENART_ART_H_
#define DEGENART_ART_H_

#include "framework.h"

extern bool g_circles;
extern UINT g_num_shapes;
extern unsigned long g_delay;

// Draws da pretty art stuffz
DWORD WINAPI ArtThread(LPVOID pvoid);

#endif // DEGENART_ART_H_
