#pragma once
#include <string.h>
#include <stdlib.h>
#include <string>
#include <iostream>

/* for given command options parser */
#ifdef _MSC_VER
#define strdup _strdup
#endif

double tempo_convert(const char* str);

void print_usage(bool fullHelp, bool isR3, std::string myName);

bool checkNumuric(char* arr, int* out);