#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <powrprof.h>
#pragma comment(lib, "PowrProf.lib")
#pragma comment(lib, "dwmapi.lib")

HWND hwndNotification = NULL;
int hFontResource = 0;
HFONT hFont = NULL;
int currentAlpha = 255;
UINT_PTR fadeTimer = 0;
NOTIFYICONDATA nid = { 0 };
bool trayIconAdded = false;
std::wstring currentNotificationText;

void InitFont();
void LoadCustomFont();
void ShowNotification(const std::wstring& text);
void AddTrayIcon(HWND hWnd);
void RemoveTrayIcon(HWND hWnd);
void DrawNotificationContent(HDC hdc, const RECT& clientRect);

LRESULT CALLBACK NotificationWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        DrawNotificationContent(hdc, clientRect);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_TIMER:
        if (wParam == 1) {
            currentAlpha -= 15;
            if (currentAlpha <= 0) {
                currentAlpha = 0;
                KillTimer(hwnd, fadeTimer);
                fadeTimer = 0;
                ShowWindow(hwnd, SW_HIDE);
            }
            SetLayeredWindowAttributes(hwnd, 0, currentAlpha, LWA_ALPHA);
        }
        return 0;

    case WM_DESTROY:
        if (hFont) {
            DeleteObject(hFont);
            hFont = NULL;
        }
        if (hFontResource != 0) {
            wchar_t path[MAX_PATH];
            GetModuleFileName(NULL, path, MAX_PATH);
            std::wstring dir(path);
            size_t pos = dir.find_last_of(L"\\/");
            if (pos != std::wstring::npos) dir = dir.substr(0, pos);
            std::wstring fontPath = dir + L"\\fonts\\ethnocentric.ttf";

            RemoveFontResourceEx(fontPath.c_str(), FR_PRIVATE | FR_NOT_ENUM, 0);
            hFontResource = 0;
        }
        return 0;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

LRESULT CALLBACK HiddenWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_USER + 1:
        switch (lParam) {
        case WM_RBUTTONUP: {
            POINT pt;
            GetCursorPos(&pt);

            HMENU hMenu = CreatePopupMenu();
            InsertMenu(hMenu, 0, MF_BYPOSITION | MF_STRING, 2, L"Edit config file");
            InsertMenu(hMenu, 1, MF_BYPOSITION | MF_STRING, 1, L"Exit");

            SetForegroundWindow(hWnd);
            TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
            PostMessage(hWnd, WM_NULL, 0, 0);
            DestroyMenu(hMenu);
            break;
        }
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {
            PostQuitMessage(0);
        }
        else if (LOWORD(wParam) == 2) {
            wchar_t path[MAX_PATH];
            GetModuleFileName(NULL, path, MAX_PATH);
            std::wstring dir(path);
            size_t pos = dir.find_last_of(L"\\/");
            if (pos != std::wstring::npos) dir = dir.substr(0, pos);

            std::wstring editPath = dir + L"\\EditConfig.exe";

            SHELLEXECUTEINFOW sei = { 0 };
            sei.cbSize = sizeof(SHELLEXECUTEINFOW);
            sei.fMask = SEE_MASK_NOCLOSEPROCESS;
            sei.hwnd = NULL;
            sei.lpVerb = L"runas";
            sei.lpFile = editPath.c_str();
            sei.lpParameters = NULL;
            sei.lpDirectory = dir.c_str();
            sei.nShow = SW_SHOWNORMAL;

            if (!ShellExecuteExW(&sei)) {
                ShowNotification(L"ТЫ КУДА ДЕЛ EditConfig.exe??");
            }
        }
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void LoadCustomFont() {
    if (hFontResource != 0) return;

    wchar_t path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);
    std::wstring dir(path);
    size_t pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos) dir = dir.substr(0, pos);

    std::wstring fontPath = dir + L"\\fonts\\ethnocentric.ttf";

    hFontResource = AddFontResourceEx(fontPath.c_str(), FR_PRIVATE | FR_NOT_ENUM, 0);
    if (hFontResource == 0) {
        ShowNotification(L"Загрузи шрифт пж");
    }
}

void InitFont() {
    if (hFont) return;
    LoadCustomFont();

    hFont = CreateFont(
        24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Ethnocentric");
}

void DrawNotificationContent(HDC hdc, const RECT& clientRect) {
    HBRUSH hBrush = CreateSolidBrush(RGB(40, 40, 40));
    FillRect(hdc, &clientRect, hBrush);
    DeleteObject(hBrush);

    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(80, 80, 80));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    HBRUSH hNullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hNullBrush);
    RoundRect(hdc, 0, 0, clientRect.right, clientRect.bottom, 15, 15);
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);

    if (hFont) {
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT textRect = { 10, 10, clientRect.right - 10, clientRect.bottom - 10 };
        DrawTextW(hdc, currentNotificationText.c_str(), -1, &textRect, DT_LEFT | DT_TOP | DT_NOPREFIX);
        SelectObject(hdc, hOldFont);
    }
}

void ShowNotification(const std::wstring& text) {
    if (!hwndNotification) {
        WNDCLASS wc = { 0 };
        wc.lpfnWndProc = NotificationWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = L"NotificationWindow";
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        RegisterClass(&wc);

        hwndNotification = CreateWindowEx(
            WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_NOACTIVATE,
            L"NotificationWindow", NULL, WS_POPUP,
            20, 20, 100, 50,
            NULL, NULL, GetModuleHandle(NULL), NULL);

        InitFont();
    }

    currentNotificationText = text;

    if (fadeTimer) {
        KillTimer(hwndNotification, fadeTimer);
        fadeTimer = 0;
    }

    HDC hdc = GetDC(hwndNotification);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    RECT rect = { 0 };
    DrawTextW(hdc, text.c_str(), -1, &rect, DT_CALCRECT | DT_NOPREFIX);
    int width = rect.right + 40;
    int height = rect.bottom + 20;
    SelectObject(hdc, hOldFont);
    ReleaseDC(hwndNotification, hdc);

    SetWindowPos(hwndNotification, HWND_TOPMOST,
        20, 20, width, height, SWP_NOACTIVATE | SWP_NOZORDER);

    currentAlpha = 255;
    SetLayeredWindowAttributes(hwndNotification, 0, currentAlpha, LWA_ALPHA);

    InvalidateRect(hwndNotification, NULL, TRUE);
    UpdateWindow(hwndNotification);

    ShowWindow(hwndNotification, SW_SHOWNOACTIVATE);
    fadeTimer = SetTimer(hwndNotification, 1, 50, NULL);
}

void AddTrayIcon(HWND hWnd) {
    wchar_t path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);
    std::wstring dir(path);
    size_t pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
        dir = dir.substr(0, pos);

    std::wstring iconPath = dir + L"\\PowerSwitcher.ico";

    HICON hIcon = (HICON)LoadImageW(NULL, iconPath.c_str(), IMAGE_ICON, 16, 16, LR_LOADFROMFILE | LR_DEFAULTSIZE);
    if (!hIcon) {
        hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_USER + 1;
    nid.hIcon = hIcon;
    wcscpy_s(nid.szTip, L"Power Scheme Switcher");

    Shell_NotifyIcon(NIM_ADD, &nid);
    trayIconAdded = true;
}

void RemoveTrayIcon(HWND hWnd) {
    if (trayIconAdded) {
        Shell_NotifyIcon(NIM_DELETE, &nid);
        trayIconAdded = false;
        if (nid.hIcon) {
            DestroyIcon(nid.hIcon);
            nid.hIcon = NULL;
        }
    }
}

std::unordered_map<std::wstring, std::wstring> LoadPowerSchemes(const std::wstring& path) {
    std::unordered_map<std::wstring, std::wstring> schemes;
    std::wifstream file(path);
    if (!file.is_open()) return schemes;

    std::wstring line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == L'#') continue;
        auto pos = line.find(L'=');
        if (pos == std::wstring::npos) continue;
        std::wstring guid = line.substr(0, pos);
        std::wstring name = line.substr(pos + 1);
        schemes[guid] = name;
    }
    return schemes;
}

bool SetPowerScheme(const std::wstring& guid) {
    GUID schemeGuid;
    HRESULT hr = CLSIDFromString(guid.c_str(), &schemeGuid);
    if (FAILED(hr)) return false;
    DWORD ret = PowerSetActiveScheme(NULL, &schemeGuid);
    return (ret == ERROR_SUCCESS);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = HiddenWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"PowerSwitcherHiddenClass";
    RegisterClass(&wc);

    HWND hHiddenWnd = CreateWindow(L"PowerSwitcherHiddenClass", NULL, 0, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);
    AddTrayIcon(hHiddenWnd);

    wchar_t path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);
    std::wstring dir(path);
    size_t pos = dir.find_last_of(L"\\/");

    if (pos != std::wstring::npos)
        dir = dir.substr(0, pos);

    std::wstring iniPath = dir + L"\\powercfg.ini";

    auto schemes = LoadPowerSchemes(iniPath);
    if (schemes.empty()) {
        MessageBox(NULL, L"Слыш, у тебя бездарный файл называется powercfg.ini", L"Йоу", MB_ICONERROR);
        RemoveTrayIcon(hHiddenWnd);
        return 1;
    }

    std::vector<std::wstring> guids;
    for (const auto& pair : schemes) guids.push_back(pair.first);

    int currentIndex = -1;

    ShowNotification(L"Press F10 for change powercfg.");

    MSG msg;
    while (true) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                RemoveTrayIcon(hHiddenWnd);
                return 0;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if ((GetAsyncKeyState(VK_F10) & 0x8000) != 0) {
            currentIndex = (currentIndex + 1) % guids.size();
            bool ok = SetPowerScheme(guids[currentIndex]);
            std::wstring notifText = ok ? schemes[guids[currentIndex]] : L"ERROR: " + schemes[guids[currentIndex]];
            ShowNotification(notifText);

            while ((GetAsyncKeyState(VK_F10) & 0x8000) != 0) Sleep(10);
        }

        Sleep(20);
    }

    RemoveTrayIcon(hHiddenWnd);
    return 0;
}
