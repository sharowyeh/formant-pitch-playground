/* generic func for plaform specific */
#pragma once
#ifdef _WIN32
#define strcpy strcpy_s
#else
#endif