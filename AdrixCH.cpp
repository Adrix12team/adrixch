// AdrixCH - A simple Windows overlay crosshair application with customizable settings.
// By Adrix12team, 2025 | License under "LICENSE" file in repository root.

#include <windows.h>
#include <thread>
#include <commctrl.h>
#include <string>
#include <shlobj.h>

std::wstring iniPath;
#pragma comment(lib, "comctl32.lib")
constexpr auto IDI_ICON1 = 101;

// Initialize variables
int g_len, g_gap, g_thickness, g_outlineThickness;
bool g_centerDot;
COLORREF g_fillColor, g_outlineColor;

HWND hBtnClose = NULL;
HWND hBtnCenter = NULL;
HWND hBtnFill = NULL;
HWND hBtnOutline = NULL;
HWND hBtnLoadCode = NULL;

HFONT g_hFont = NULL;

HWND hLabelLen = NULL;
HWND hLabelGap = NULL;
HWND hLabelThickness = NULL;
HWND hLabelOutline = NULL;

HWND hCrosshairCodeInput = NULL;

// Forward declarations
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SettingsProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void OpenSettingsWindow(HINSTANCE hInstance);
void DrawCrosshair(HDC hdc, int cx, int cy, int len, int gap, int thickness, COLORREF fillColor, COLORREF outlineColor, bool centerDot, int outlineThickness);

HWND hwndMain = NULL;

// Save and load helpers for INI file
void SaveSetting(const std::wstring& path, const std::wstring& section, const std::wstring& key, const std::wstring& value) {
    WritePrivateProfileStringW(section.c_str(), key.c_str(), value.c_str(), path.c_str());
}
std::wstring LoadSetting(const std::wstring& path, const std::wstring& section, const std::wstring& key, const std::wstring& defaultValue = L"") {
    wchar_t buffer[256];
    GetPrivateProfileStringW(section.c_str(), key.c_str(), defaultValue.c_str(), buffer, 256, path.c_str());
    return buffer;
}

void LoadCrosshairSettings() {
    // Load settings from INI file or use defaults
    g_len = std::stoi(LoadSetting(iniPath, L"Crosshair", L"Length", L"7"));
    g_gap = std::stoi(LoadSetting(iniPath, L"Crosshair", L"GapSize", L"1"));
    g_thickness = std::stoi(LoadSetting(iniPath, L"Crosshair", L"Thickness", L"2"));
    g_outlineThickness = std::stoi(LoadSetting(iniPath, L"Crosshair", L"OutlineThickness", L"1"));
    g_centerDot = (LoadSetting(iniPath, L"Crosshair", L"CenterDot", L"0") == L"1");
    g_fillColor = std::stoi(LoadSetting(iniPath, L"Crosshair", L"FillColor", L"65535"));
    g_outlineColor = std::stoi(LoadSetting(iniPath, L"Crosshair", L"OutlineColor", L"65536"));
}

// Encode and decode crosshair settings into a compact string
std::wstring GetCrosshairCode() {
    wchar_t buf[32];
    swprintf(buf, 32, L"%02d%02d%02d%02d%d-%06X-%06X",
        g_len, g_gap, g_thickness, g_outlineThickness,
        g_centerDot ? 1 : 0,
        g_fillColor & 0xFFFFFF,
        g_outlineColor & 0xFFFFFF
    );
    return std::wstring(buf);
}
void LoadCrosshairCode(const std::wstring& code) {
    int len, gap, thick, outline, dot;
    unsigned int fill, outlineCol;

    swscanf_s(code.c_str(), L"%2d%2d%2d%2d%d-%6X-%6X",
        &len, &gap, &thick, &outline, &dot, &fill, &outlineCol);

    g_len = len;
    g_gap = gap;
    g_thickness = thick;
    g_outlineThickness = outline;
    g_centerDot = dot != 0;
    g_fillColor = fill;
    g_outlineColor = outlineCol;
}

bool IsValidCrosshairCode(const std::wstring& code) {
    if (code.length() != 23) return false; // 9 digits + '-' + 6 Hex + '-' + 6 Hex = 23 chars
    if (code[9] != '-' || code[16] != '-') return false;

	// Check if the first 9 characters are digits
    for (int i = 0; i < 9; i++) {
        if (!iswdigit(code[i])) return false;
    }

	// Center dot should be '0' or '1'
    if (code[8] != L'0' && code[8] != L'1') return false;

	// Check if the hex parts are valid hexadecimal digits
    for (int i = 10; i < 16; i++) {
        if (!iswxdigit(code[i])) return false;
    }
    for (int i = 17; i < 23; i++) {
        if (!iswxdigit(code[i])) return false;
    }

    return true;
}

void DrawCrosshair(HDC hdc, int cx, int cy, int len, int gap, int thickness, COLORREF fillColor, COLORREF outlineColor, bool centerDot, int outlineThickness) {
    auto DrawRectWithOutline = [&](RECT r) {
        // Fill color
        HBRUSH hBrush = CreateSolidBrush(fillColor);
        FillRect(hdc, &r, hBrush);
        DeleteObject(hBrush);

        // Draw outline only if outlineThickness > 0
        if (outlineThickness > 0) {
            HPEN hPen = CreatePen(PS_SOLID, outlineThickness, outlineColor);
            HGDIOBJ oldPen = SelectObject(hdc, hPen);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, r.left, r.top, r.right, r.bottom);
            SelectObject(hdc, oldPen);
            SelectObject(hdc, hOldBrush);
            DeleteObject(hPen);
        }
    };

	// Compute rectangles for each arm of the crosshair, this is where the magic happens
    RECT left   = { cx - len - gap, cy - thickness / 2, cx - gap, cy + thickness / 2 };
    RECT right  = { cx + gap, cy - thickness / 2, cx + len + gap, cy + thickness / 2 };
    RECT top    = { cx - thickness / 2, cy - len - gap, cx + thickness / 2, cy - gap };
    RECT bottom = { cx - thickness / 2, cy + gap, cx + thickness / 2, cy + len + gap };

    DrawRectWithOutline(left); DrawRectWithOutline(right); DrawRectWithOutline(top); DrawRectWithOutline(bottom);

    // Optional center dot drawn as a filled rectangle with outline
    if (centerDot) { RECT dot = { cx - thickness / 2, cy - thickness / 2, cx + thickness / 2, cy + thickness / 2 }; DrawRectWithOutline(dot); }
}

// Main overlay window procedure: paints transparent fullscreen and draws the crosshair
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Fill background with the color key (this will be treated as transparent due to SetLayeredWindowAttributes)
        RECT fullScreen = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
        HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &fullScreen, hBrush);
        DeleteObject(hBrush);

        // Draw the crosshair centered on the primary monitor
        int cx = GetSystemMetrics(SM_CXSCREEN) / 2;
        int cy = GetSystemMetrics(SM_CYSCREEN) / 2;
        DrawCrosshair(hdc, cx, cy, g_len, g_gap, g_thickness * 2, g_fillColor, g_outlineColor, g_centerDot, g_outlineThickness);

        EndPaint(hwnd, &ps);
        break;
    }
    case WM_DESTROY: PostQuitMessage(0); break;
    default: return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// Settings window procedure: handles control events, color choosers, sliders and owner-drawn buttons
LRESULT CALLBACK SettingsProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case 1: { // Close button: persist settings and close both settings and main overlay
            SaveSetting(iniPath, L"Crosshair", L"Length", std::to_wstring(g_len));
            SaveSetting(iniPath, L"Crosshair", L"GapSize", std::to_wstring(g_gap));
            SaveSetting(iniPath, L"Crosshair", L"Thickness", std::to_wstring(g_thickness));
            SaveSetting(iniPath, L"Crosshair", L"OutlineThickness", std::to_wstring(g_outlineThickness));
            SaveSetting(iniPath, L"Crosshair", L"CenterDot", g_centerDot ? L"1" : L"0");
            SaveSetting(iniPath, L"Crosshair", L"FillColor", std::to_wstring(static_cast<unsigned long>(g_fillColor)));
            SaveSetting(iniPath, L"Crosshair", L"OutlineColor", std::to_wstring(static_cast<unsigned long>(g_outlineColor)));
            if (hwndMain && IsWindow(hwndMain)) { SendMessage(hwndMain, WM_CLOSE, 0, 0); } DestroyWindow(hwnd);
            break;
        }
        case 2: { // Toggle the center dot setting and request repaint
            g_centerDot = !g_centerDot;
            // Update code string shown to user
            SetWindowText(hCrosshairCodeInput, GetCrosshairCode().c_str());
            InvalidateRect(hwndMain, NULL, TRUE);
            InvalidateRect(hwnd, NULL, TRUE); // refresh button text
            break;
        }
        case 3: { // Open color chooser for fill color
            CHOOSECOLOR cc = {};
            COLORREF custom[16] = { 0 };
            cc.lStructSize = sizeof(cc);
            cc.hwndOwner = hwnd;
            cc.rgbResult = g_fillColor;
            cc.lpCustColors = custom;
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;
            if (ChooseColor(&cc)) {
                if (cc.rgbResult == RGB(0, 0, 0)) { cc.rgbResult = RGB(0, 0, 1); /* Lightly adjust black to avoid invisibility */ }
                g_fillColor = cc.rgbResult;
                // Update code string shown to user
                SetWindowText(hCrosshairCodeInput, GetCrosshairCode().c_str());
                InvalidateRect(hwndMain, NULL, TRUE);
            }
            break;
        }
        case 4: { // Open color chooser for outline color
            CHOOSECOLOR cc = {};
            COLORREF custom[16] = { 0 };
            cc.lStructSize = sizeof(cc);
            cc.hwndOwner = hwnd;
            cc.rgbResult = g_outlineColor;
            cc.lpCustColors = custom;
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;
            if (ChooseColor(&cc)) {
                if (cc.rgbResult == RGB(0, 0, 0)) { cc.rgbResult = RGB(0, 0, 1); /* Same here */ }
                g_outlineColor = cc.rgbResult;
                // Update code string shown to user
                SetWindowText(hCrosshairCodeInput, GetCrosshairCode().c_str());
                InvalidateRect(hwndMain, NULL, TRUE);
            }
            break;
        }
        case 5: { // Load crosshair from code input
            wchar_t buf[256];
            GetWindowText(hCrosshairCodeInput, buf, 256);
            std::wstring code(buf);

            if (!IsValidCrosshairCode(code)) {
                MessageBox(hwnd, L"Invalid Crosshair Code!", L"Error", MB_OK | MB_ICONERROR);
				break; // Break if crosshair code is invalid
            }

			// Working code, load settings
            LoadCrosshairCode(code);

            // Update Slider & Labels
            SendMessage(GetDlgItem(hwnd, 101), TBM_SETPOS, TRUE, g_len);
            SendMessage(GetDlgItem(hwnd, 102), TBM_SETPOS, TRUE, g_thickness);
            SendMessage(GetDlgItem(hwnd, 103), TBM_SETPOS, TRUE, g_outlineThickness);
            SendMessage(GetDlgItem(hwnd, 104), TBM_SETPOS, TRUE, g_gap);

			// Update code string shown to user
            SetWindowText(hCrosshairCodeInput, GetCrosshairCode().c_str());

            wchar_t labelBuf[32];
            swprintf(labelBuf, 32, L"Length: %d", g_len); SetWindowText(hLabelLen, labelBuf);
            swprintf(labelBuf, 32, L"Thickness: %d", g_thickness); SetWindowText(hLabelThickness, labelBuf);
            swprintf(labelBuf, 32, L"Outline: %d", g_outlineThickness); SetWindowText(hLabelOutline, labelBuf);
            swprintf(labelBuf, 32, L"Gap: %d", g_gap); SetWindowText(hLabelGap, labelBuf);

            InvalidateRect(hwndMain, NULL, TRUE);
            InvalidateRect(hwnd, NULL, TRUE); // refresh toggle button text
            break;
        }
        }
        break;

    case WM_HSCROLL: {
        int pos = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
        wchar_t buf[32];
        // Update crosshair code when sliders change
        SetWindowText(hCrosshairCodeInput, GetCrosshairCode().c_str());
        switch (GetDlgCtrlID((HWND)lParam)) {
        case 101: g_len = pos; swprintf(buf, 32, L"Length: %d", g_len); SetWindowText(hLabelLen, buf); break;
        case 102: g_thickness = pos; swprintf(buf, 32, L"Thickness: %d", g_thickness); SetWindowText(hLabelThickness, buf); break;
        case 103: g_outlineThickness = pos; swprintf(buf, 32, L"Outline: %d", g_outlineThickness); SetWindowText(hLabelOutline, buf); break;
        case 104: g_gap = pos; swprintf(buf, 32, L"Gap: %d", g_gap); SetWindowText(hLabelGap, buf); break;
        }
        InvalidateRect(hwndMain, NULL, TRUE);
        break;
    }

    case WM_CTLCOLORSTATIC: {
        // Static labels: white text on black background
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, RGB(255, 255, 255));
        SetBkColor(hdcStatic, RGB(0, 0, 0));
        // Return a brush matching background (created here, but system will free on WM_DESTROY of control)
        return (INT_PTR)CreateSolidBrush(RGB(0, 0, 0));
    }

    case WM_CTLCOLORBTN: {
        // Buttons: white text and transparent background to allow owner-draw style
        HDC hdcBtn = (HDC)wParam;
        SetTextColor(hdcBtn, RGB(255, 255, 255));
        SetBkMode(hdcBtn, TRANSPARENT);
        return (INT_PTR)GetStockObject(NULL_BRUSH);
    }

    case WM_DRAWITEM: {
        // Owner-drawn buttons: draw a dark rounded rectangle and centered white text
        LPDRAWITEMSTRUCT lpDIS = (LPDRAWITEMSTRUCT)lParam;
        if (!lpDIS) break;

        // Background fill
        HBRUSH hBrush = CreateSolidBrush(RGB(30, 30, 30));
        FillRect(lpDIS->hDC, &lpDIS->rcItem, hBrush);
        DeleteObject(hBrush);

        // Draw rounded rectangle borderless (we use NULL_PEN)
        HPEN hPen = (HPEN)GetStockObject(NULL_PEN);
        HGDIOBJ oldPen = SelectObject(lpDIS->hDC, hPen);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(lpDIS->hDC, CreateSolidBrush(RGB(30, 30, 30)));
        RoundRect(lpDIS->hDC, lpDIS->rcItem.left, lpDIS->rcItem.top, lpDIS->rcItem.right, lpDIS->rcItem.bottom, 10, 10);
        SelectObject(lpDIS->hDC, oldPen);
        SelectObject(lpDIS->hDC, hOldBrush);

        // Text: center and vertical align
        SetBkMode(lpDIS->hDC, TRANSPARENT);
        SetTextColor(lpDIS->hDC, RGB(255, 255, 255));

        if (lpDIS->CtlID == 2) { // Toggle Button Text
            wchar_t buf[64];
            swprintf(buf, 64, L"Toggle Center Dot: %s", g_centerDot ? L"ON" : L"OFF");
            DrawText(lpDIS->hDC, buf, -1, &lpDIS->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        else {
            wchar_t buf[64];
            GetWindowText(lpDIS->hwndItem, buf, 64);
            DrawText(lpDIS->hDC, buf, -1, &lpDIS->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        return TRUE;
    }

    default: return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// Create and run the settings window modally (blocks until closed)
void OpenSettingsWindow(HINSTANCE hInstance) {
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    const wchar_t CLASS_NAME[] = L"SettingsWin";
    WNDCLASS wc = {};
    wc.lpfnWndProc = SettingsProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
    wc.hIcon = (HICON)LoadImage(hInstance, L"AdrixCH.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE | LR_SHARED);
    RegisterClass(&wc);

	// Client window size
    int clientWidth = 325;
    int clientHeight = 340;

    RECT rc = { 0, 0, clientWidth, clientHeight };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_SIZEBOX, FALSE);

	// Real window size after adjustment
    int winWidth = rc.right - rc.left;
    int winHeight = rc.bottom - rc.top;

    HWND hwndSettings = CreateWindowEx(
        WS_EX_TOPMOST, CLASS_NAME, L"AdrixCH - Settings",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_SIZEBOX,
        200, 200, winWidth, winHeight,
        NULL, NULL, hInstance, NULL
    );

    // Create a readable font for controls
    g_hFont = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

    // Buttons (owner-drawn)
    hBtnClose = CreateWindow(L"BUTTON", L"Close", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 10, 10, 147, 30, hwndSettings, (HMENU)1, hInstance, NULL);
    hBtnCenter = CreateWindow(L"BUTTON", L"Toggle Center Dot", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 167, 10, 147, 30, hwndSettings, (HMENU)2, hInstance, NULL);
    hBtnFill = CreateWindow(L"BUTTON", L"Fill Color", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 10, 50, 147, 30, hwndSettings, (HMENU)3, hInstance, NULL);
    hBtnOutline = CreateWindow(L"BUTTON", L"Outline Color", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 167, 50, 147, 30, hwndSettings, (HMENU)4, hInstance, NULL);
    hBtnLoadCode = CreateWindow(L"BUTTON", L"Load Crosshair", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 220, 300, 95, 30, hwndSettings, (HMENU)5, hInstance, NULL);

    // Trackbars
    HWND hLen = CreateWindowEx(0, TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 10, 100, 200, 30, hwndSettings, (HMENU)101, hInstance, NULL);
    SendMessage(hLen, TBM_SETRANGE, TRUE, MAKELONG(2, 50));
    SendMessage(hLen, TBM_SETPOS, TRUE, g_len);
    hLabelLen = CreateWindow(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 220, 100, 105, 30, hwndSettings, NULL, hInstance, NULL);

    HWND hThick = CreateWindowEx(0, TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 10, 150, 200, 30, hwndSettings, (HMENU)102, hInstance, NULL);
    SendMessage(hThick, TBM_SETRANGE, TRUE, MAKELONG(1, 20));
    SendMessage(hThick, TBM_SETPOS, TRUE, g_thickness);
    hLabelThickness = CreateWindow(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 220, 150, 105, 30, hwndSettings, NULL, hInstance, NULL);

    HWND hOutline = CreateWindowEx(0, TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 10, 200, 200, 30, hwndSettings, (HMENU)103, hInstance, NULL);
    SendMessage(hOutline, TBM_SETRANGE, TRUE, MAKELONG(0, 10));
    SendMessage(hOutline, TBM_SETPOS, TRUE, g_outlineThickness);
    hLabelOutline = CreateWindow(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 220, 200, 105, 30, hwndSettings, NULL, hInstance, NULL);

    HWND hGap = CreateWindowEx(0, TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 10, 250, 200, 30, hwndSettings, (HMENU)104, hInstance, NULL);
    SendMessage(hGap, TBM_SETRANGE, TRUE, MAKELONG(0, 50));
    SendMessage(hGap, TBM_SETPOS, TRUE, g_gap);
    hLabelGap = CreateWindow(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 220, 250, 105, 30, hwndSettings, NULL, hInstance, NULL);

    hCrosshairCodeInput = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 10, 300, 200, 30, hwndSettings, (HMENU)201 ,hInstance, NULL);

    HWND controls[] = { hBtnClose, hBtnCenter, hBtnFill, hBtnOutline, hBtnLoadCode, hLabelLen, hLabelThickness, hLabelOutline, hLabelGap, hLen, hThick, hOutline, hGap, hCrosshairCodeInput };
    for (HWND ctrl : controls) { SendMessage(ctrl, WM_SETFONT, (WPARAM)g_hFont, TRUE); }

    wchar_t buf[32];
    swprintf(buf, 32, L"Length: %d", g_len); SetWindowText(hLabelLen, buf);
    swprintf(buf, 32, L"Thickness: %d", g_thickness); SetWindowText(hLabelThickness, buf);
    swprintf(buf, 32, L"Outline: %d", g_outlineThickness); SetWindowText(hLabelOutline, buf);
    swprintf(buf, 32, L"Gap: %d", g_gap); SetWindowText(hLabelGap, buf);

    SetWindowText(hCrosshairCodeInput, GetCrosshairCode().c_str());

    ShowWindow(hwndSettings, SW_SHOW); UpdateWindow(hwndSettings);

    // Message loop for settings window (modal-like behavior)
    MSG msg;
    while (IsWindow(hwndSettings) && GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
}

// Background thread: listens for F12 to open settings window
void KeyCheckThread(HINSTANCE hInstance) {
    while (true) {
        if (GetAsyncKeyState(VK_F12) & 0x8000) { OpenSettingsWindow(hInstance); Sleep(500); /* simple debounce to avoid rapid repeated opens */ } Sleep(50);
    }
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"AdrixCH";

    // Resolve AppData path and INI file location
    wchar_t appData[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appData);
    iniPath = std::wstring(appData) + L"\\AdrixCH\\crosshair_settings.ini";

    // Ensure configuration directory exists
    CreateDirectoryW((std::wstring(appData) + L"\\AdrixCH").c_str(), NULL);

    // Load persisted settings (if any)
    LoadCrosshairSettings();

    // Register overlay window class
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    
    // Attempt to load icon from file; if missing, LoadImage returns NULL and system defaults apply
    wc.hIcon = (HICON)LoadImage(hInstance, L"AdrixCH.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE | LR_SHARED);
    wc.hIconSm = (HICON)LoadImage(hInstance, L"AdrixCH.ico", IMAGE_ICON, 64, 64, LR_LOADFROMFILE | LR_SHARED);
    RegisterClassEx(&wc);

    // Create an always-on-top, layered, transparent window that covers the screen
    hwndMain = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        CLASS_NAME, L"AdrixCH", WS_POPUP,
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        NULL, NULL, hInstance, NULL
    );

    // Use color-key transparency for the background (RGB(0,0,0) will be transparent)
    SetLayeredWindowAttributes(hwndMain, RGB(0, 0, 0), 255, LWA_COLORKEY | LWA_ALPHA);
    ShowWindow(hwndMain, SW_SHOW);

    // Start key-check thread to open settings with F12
    std::thread(KeyCheckThread, hInstance).detach();

    // Standard message loop for main window
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }

    return 0;
}