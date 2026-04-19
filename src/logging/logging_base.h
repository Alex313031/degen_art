#ifndef MINI_LOGGER_LOGGING_BASE_H_
#define MINI_LOGGER_LOGGING_BASE_H_

// NOTE: This is a precompiled header file (PCH)

// clang-format off
#include <windows.h> // Main Windows include
#include <wincon.h>  // Console API functions
#include <tchar.h>   // Wide/Short characters
// clang-format on

// For strings, writing to console/file, etc.
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

namespace logging {
  extern bool logging_initialized; // Global boolean for safety
}

#endif // MINI_LOGGER_LOGGING_BASE_H_
