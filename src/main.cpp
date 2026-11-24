#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <string>
#include <vector>
#include "resource.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "ole32.lib")

using namespace Gdiplus;
static ULONG_PTR g_gdiplus = 0;
static Image* g_splash = nullptr;
static IStream* g_gifStream = nullptr;
static PrivateFontCollection g_fonts;
static FontFamily* g_fontFamilies = nullptr;
static FontFamily* g_fontFamily = nullptr;
static int g_frame = 0;
static UINT g_frameCount = 1;
static int g_progress = 0;
static int g_phase = 0;
static bool g_dragging = false;
static POINT g_dragPoint{};

static std::vector<BYTE> LoadResourceBytes(int id, const wchar_t* type) {
    HRSRC resource = FindResourceW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(id), type);
    if (!resource) return {};
    HGLOBAL loaded = LoadResource(GetModuleHandleW(nullptr), resource);
    DWORD size = SizeofResource(GetModuleHandleW(nullptr), resource);
    const BYTE* data = static_cast<const BYTE*>(LockResource(loaded));
    if (!data || size == 0) return {};
    return std::vector<BYTE>(data, data + size);
}

static void InitializeAssets() {
    auto gif = LoadResourceBytes(IDR_SPLASH_GIF, L"GIF");
    if (!gif.empty()) {
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, gif.size());
        if (memory) {
            void* target = GlobalLock(memory);
            if (target) {
                CopyMemory(target, gif.data(), gif.size());
                GlobalUnlock(memory);
                if (SUCCEEDED(CreateStreamOnHGlobal(memory, TRUE, &g_gifStream))) {
                    g_splash = Image::FromStream(g_gifStream, FALSE);
                    if (g_splash && g_splash->GetLastStatus() == Ok) {
                        g_frameCount = g_splash->GetFrameCount(&FrameDimensionTime);
                        if (!g_frameCount) g_frameCount = 1;
                    } else {
                        delete g_splash;
                        g_splash = nullptr;
                    }
                } else GlobalFree(memory);
            } else GlobalFree(memory);
        }
    }
    auto font = LoadResourceBytes(IDR_FORGE_FONT, L"FONT");
    if (!font.empty() && g_fonts.AddMemoryFont(font.data(), static_cast<INT>(font.size())) == Ok) {
        INT count = 0;
        g_fontFamilies = new FontFamily[1];
        g_fonts.GetFamilies(1, g_fontFamilies, &count);
        if (count) g_fontFamily = &g_fontFamilies[0];
        else { delete[] g_fontFamilies; g_fontFamilies = nullptr; }
    }
}

static void DrawText(Graphics& graphics, const std::wstring& value, float x, float y, float size, Color color, INT style = FontStyleRegular) {
    FontFamily fallback(L"Segoe UI");
    const FontFamily& family = g_fontFamily ? *g_fontFamily : fallback;
    Font font(&family, size, style, UnitPixel);
    SolidBrush brush(color);
    graphics.DrawString(value.c_str(), static_cast<INT>(value.size()), &font, PointF(x, y), &brush);
}

static void DrawRounded(Graphics& graphics, float x, float y, float width, float height, float radius, Color color) {
    GraphicsPath path;
    path.AddArc(x, y, radius * 2, radius * 2, 180, 90);
    path.AddArc(x + width - radius * 2, y, radius * 2, radius * 2, 270, 90);
    path.AddArc(x + width - radius * 2, y + height - radius * 2, radius * 2, radius * 2, 0, 90);
    path.AddArc(x, y + height - radius * 2, radius * 2, radius * 2, 90, 90);
    path.CloseFigure();
    SolidBrush brush(color);
    graphics.FillPath(&brush, &path);
}

static void Paint(HWND hwnd) {
    PAINTSTRUCT ps{};
    HDC target = BeginPaint(hwnd, &ps);
    RECT client{};
    GetClientRect(hwnd, &client);
    const int width = client.right;
    const int height = client.bottom;
    HDC buffer = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, width, height);
    HGDIOBJ oldBitmap = SelectObject(buffer, bitmap);
    Graphics graphics(buffer);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    graphics.Clear(Color(255, 12, 13, 17));
    LinearGradientBrush background(Rect(0, 0, width, height), Color(255, 19, 20, 26), Color(255, 9, 10, 13), LinearGradientModeVertical);
    graphics.FillRectangle(&background, 0, 0, width, height);
    SolidBrush glow(Color(24, 255, 70, 34));
    graphics.FillEllipse(&glow, -130, 245, 430, 430);
    SolidBrush glow2(Color(13, 255, 45, 20));
    graphics.FillEllipse(&glow2, 550, -240, 470, 470);
    Pen border(Color(255, 49, 43, 43), 1.0f);
    graphics.DrawRectangle(&border, 0, 0, width - 1, height - 1);
    SolidBrush accent(Color(255, 235, 76, 39));
    graphics.FillRectangle(&accent, 0, 0, width, 3);

    DrawText(graphics, L"DRAGON", 52, 54, 35, Color(255, 246, 244, 242), FontStyleBold);
    DrawText(graphics, L"ANTI-CHEAT", 54, 99, 14, Color(255, 231, 91, 51), FontStyleBold);
    DrawText(graphics, L"ADVANCED GAME PROTECTION", 55, 132, 10, Color(255, 120, 120, 127));
    DrawText(graphics, L"INITIALIZING SECURITY SYSTEM", 258, 298, 16, Color(255, 238, 236, 233), FontStyleBold);
    DrawText(graphics, L"Preparing Dragon protection modules", 259, 326, 11, Color(255, 139, 139, 146));
    DrawText(graphics, L"PLEASE WAIT", 259, 356, 9, Color(255, 231, 91, 51), FontStyleBold);

    if (g_splash) {
        if (g_frameCount > 1) g_splash->SelectActiveFrame(&FrameDimensionTime, g_frame);
        const UINT sw = g_splash->GetWidth();
        const UINT sh = g_splash->GetHeight();
        if (sw && sh) {
            const float scale = std::min(190.0f / sw, 166.0f / sh);
            const float dw = sw * scale;
            const float dh = sh * scale;
            graphics.DrawImage(g_splash, RectF(42.0f + (190.0f - dw) / 2.0f, 222.0f + (166.0f - dh) / 2.0f, dw, dh));
        }
    }

    DrawText(graphics, L"LOADING", 55, 423, 10, Color(255, 153, 151, 153), FontStyleBold);
    const std::wstring percentage = std::to_wstring(g_progress) + L"%";
    FontFamily fallback(L"Segoe UI");
    const FontFamily& family = g_fontFamily ? *g_fontFamily : fallback;
    Font percentFont(&family, 17.0f, FontStyleBold, UnitPixel);
    SolidBrush percentBrush(Color(255, 244, 91, 49));
    RectF percentRect{};
    graphics.MeasureString(percentage.c_str(), -1, &percentFont, PointF(0, 0), &percentRect);
    graphics.DrawString(percentage.c_str(), -1, &percentFont, PointF(width - 54.0f - percentRect.Width, 417.0f), &percentBrush);
    DrawRounded(graphics, 54, 448, width - 108, 8, 4, Color(255, 43, 43, 49));
    if (g_progress > 0) {
        DrawRounded(graphics, 54, 448, (width - 108) * (g_progress / 100.0f), 8, 4, Color(255, 237, 77, 42));
        DrawRounded(graphics, 54, 448, std::max(8.0f, (width - 108) * (g_progress / 100.0f)), 3, 2, Color(150, 255, 174, 113));
    }
    DrawText(graphics, L"DRAGON ANTI-CHEAT", 54, 477, 9, Color(255, 104, 104, 111), FontStyleBold);
    DrawText(graphics, L"SECURE INITIALIZATION", width - 190, 477, 9, Color(255, 104, 104, 111));
    const int closeX = width - 42;
    const int closeY = 22;
    Pen closePen(Color(255, 170, 166, 166), 1.6f);
    graphics.DrawLine(&closePen, closeX, closeY, closeX + 12, closeY + 12);
    graphics.DrawLine(&closePen, closeX + 12, closeY, closeX, closeY + 12);

    BitBlt(target, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
    SelectObject(buffer, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(buffer);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_PAINT: Paint(hwnd); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_TIMER:
        if (wParam == 1) {
            if (g_frameCount > 1) {
                g_frame = (g_frame + 1) % static_cast<int>(g_frameCount);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        } else if (wParam == 2) {
            if (g_progress < 92) ++g_progress;
            else if (g_progress < 100 && ++g_phase > 12) g_progress = 100;
            if (g_progress >= 100) {
                KillTimer(hwnd, 2);
                SetTimer(hwnd, 3, 700, nullptr);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (wParam == 3) {
            KillTimer(hwnd, 3);
            g_progress = 0;
            g_phase = 0;
            SetTimer(hwnd, 2, 65, nullptr);
        }
        return 0;
    case WM_LBUTTONDOWN: {
        POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        RECT client{};
        GetClientRect(hwnd, &client);
        if (point.y <= 52 && point.x >= client.right - 64) {
            DestroyWindow(hwnd);
            return 0;
        }
        if (point.y <= 190) {
            g_dragging = true;
            g_dragPoint = point;
            SetCapture(hwnd);
        }
        return 0;
    }
    case WM_MOUSEMOVE:
        if (g_dragging && (wParam & MK_LBUTTON)) {
            POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            POINT screen = point;
            ClientToScreen(hwnd, &screen);
            SetWindowPos(hwnd, nullptr, screen.x - g_dragPoint.x, screen.y - g_dragPoint.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        return 0;
    case WM_LBUTTONUP: g_dragging = false; ReleaseCapture(); return 0;
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            POINT point{};
            GetCursorPos(&point);
            ScreenToClient(hwnd, &point);
            RECT client{};
            GetClientRect(hwnd, &client);
            if (point.y <= 52 && point.x >= client.right - 64) {
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
        }
        break;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        KillTimer(hwnd, 2);
        KillTimer(hwnd, 3);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    GdiplusStartupInput startup;
    if (GdiplusStartup(&g_gdiplus, &startup, nullptr) != Ok) return 1;
    InitializeAssets();
    const wchar_t className[] = L"DragonAntiCheatSplashWindow";
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = className;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    if (!RegisterClassExW(&wc)) return 2;
    const int width = 760;
    const int height = 540;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    HWND window = CreateWindowExW(WS_EX_APPWINDOW, className, L"DRAGON ANTI-CHEAT", WS_POPUP, x, y, width, height, nullptr, nullptr, instance, nullptr);
    if (!window) return 3;
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    SetTimer(window, 1, 45, nullptr);
    SetTimer(window, 2, 65, nullptr);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    delete g_splash;
    if (g_gifStream) g_gifStream->Release();
    delete[] g_fontFamilies;
    GdiplusShutdown(g_gdiplus);
    return static_cast<int>(message.wParam);
}
