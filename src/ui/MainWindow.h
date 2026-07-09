#pragma once

#include <Windows.h>

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool Create();
    void Show() const;
    int RunMessageLoop() const;
    void Destroy();

private:
    static LRESULT CALLBACK WndProc(
        HWND hwnd,
        UINT msg,
        WPARAM wParam,
        LPARAM lParam
    );

    HWND m_hwnd;
    HINSTANCE m_hInstance;

    static constexpr const wchar_t* kWindowClassName = L"YukiMainWindowClass";
    static constexpr const wchar_t* kWindowTitle = L"Yuki";
};