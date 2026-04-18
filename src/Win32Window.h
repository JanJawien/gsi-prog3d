#pragma once
#include <windows.h>

class Dx12App;

namespace Win32Window
{
    void RegisterWindowClass(HINSTANCE hInstance);
    HWND CreateAppWindow(HINSTANCE hInstance, int nCmdShow, Dx12App* app);
}