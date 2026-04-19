#ifndef DEGENART_MAIN_H_
#define DEGENART_MAIN_H_

#include "art.h"
#include "globals.h"

// Main window procedure
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

// Initializes app state and launches art thread
bool InitApp(HWND hWnd);

// Closes all windows and cleans up any resources.
void ShutDownApp();

// Shows help
bool LaunchHelp(HWND hWnd);

// About dialog handler
INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

#endif // DEGENART_MAIN_H_
