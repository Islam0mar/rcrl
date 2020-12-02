#pragma once

#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 0

#ifdef _WIN32
#define RCRL_EXPORT_API __declspec(dllexport)
#define RCRL_IMPORT_API __declspec(dllimport)
#else
#define RCRL_EXPORT_API __attribute__((visibility("default")))
#define RCRL_IMPORT_API
#endif

// From
// https://stackoverflow.com/questions/5459868/concatenate-int-to-string-using-c-preprocessor
#define __STR_HELPER(x) #x
#define __STR(x) __STR_HELPER(x)
#define VERSION_STRING \
  __STR(VERSION_MAJOR) \
  "." __STR(VERSION_MINOR) "." __STR(VERSION_PATCH)
