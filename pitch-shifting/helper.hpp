#pragma once
#include <string.h>
#include <stdlib.h>
#include <string>
#include <iostream>

/* for given command options parser */
#ifdef _MSC_VER
#define strdup _strdup
#endif

#include <thread>
#include <atomic>
#include <conio.h> // to design thread awareness wait key func (mainly to cancel audio stream realtime works)
#ifdef _WIN32
#define getch _getch
#endif

double tempo_convert(const char* str);

void print_usage(bool fullHelp, bool isR3, std::string myName);

bool checkNumuric(char* arr, int* out);

bool isWaitKeyPressed();
void setWaitKey(int keyCode);