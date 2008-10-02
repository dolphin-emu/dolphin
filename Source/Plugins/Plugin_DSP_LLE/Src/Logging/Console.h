#ifdef _WIN32

void startConsoleWin(int width, int height, char* fname);
int wprintf(char *fmt, ...);
void ClearScreen();
HWND GetConsoleHwnd(void);

#endif