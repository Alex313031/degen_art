#ifndef DEGENART_FRAMEWORK_H_
#define DEGENART_FRAMEWORK_H_

#include "version.h"

#include <windows.h>
#include <commctrl.h>
#include <process.h>

#include <tchar.h>

#include <iostream>
#include <string>
#include <thread>

#ifndef __FUNC__
 #define __FUNC__ __func__
#endif

inline constexpr bool is_dcheck =
#ifdef DCHECK_ON
    true;
#else
    false;
#endif // DCHECK

inline constexpr bool is_debug =
#if defined(DEBUG) || defined(_DEBUG)
    true;
#else
    false;
#endif // defined(DEBUG) || defined(_DEBUG)

#endif // DEGENART_FRAMEWORK_H_
