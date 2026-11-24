#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <string>
#include <vector>
#include "embedded_assets.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "ole32.lib")

using namespace Gdiplus;
static ULONG_PTR g_gdiplus = 0;
static std::vector<Image*> g_frames;
static std::vector<IStream*> g_frameStreams;
static PrivateFontCollection g_fonts;
static FontFamily* g_fontFamilies = nullptr;
static FontFamily* g_fontFamily = nullptr;
static int g_frame = 0;
static int g_progress = 0;
static int g_phase = 0;
static bool g_dragging = false;
static POINT g_dragPoint{};

static void InitializeAssets() {
    for (const EmbeddedFrame& frame : kSplashFrames) {
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, frame.size);
        if (!memory) continue;
        void* target = GlobalLock(memory);
        if (!target) { GlobalFree(memory); continue; }
        CopyMemory(target, frame.data, frame.size);
        GlobalUnlock(memory);
        IStream* stream = nullptr;
        if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream))) { GlobalFree(memory); continue; }
        Image* image = Image::FromStream(stream, FALSE);
        if (!image || image->GetLastStatus() != Ok) { delete image; stream->Release(); continue; }
        g_frameStreams.push_back(stream);
        g_frames.push_back(image);
    }
    if (g_fonts.AddMemoryFont(kForgeFont, static_cast<INT>(sizeof(kForgeFont))) == Ok) {
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

    if (!g_frames.empty()) {
        Image* splash = g_frames[static_cast<size_t>(g_frame) % g_frames.size()];
        const UINT sw = splash->GetWidth();
        const UINT sh = splash->GetHeight();
        if (sw && sh) {
            const float scale = std::min(190.0f / static_cast<float>(sw), 166.0f / static_cast<float>(sh));
            const float dw = static_cast<float>(sw) * scale;
            const float dh = static_cast<float>(sh) * scale;
            graphics.DrawImage(splash, RectF(42.0f + (190.0f - dw) / 2.0f, 222.0f + (166.0f - dh) / 2.0f, dw, dh));
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
    DrawRounded(graphics, 54.0f, 448.0f, static_cast<float>(width - 108), 8.0f, 4.0f, Color(255, 43, 43, 49));
    if (g_progress > 0) {
        DrawRounded(graphics, 54.0f, 448.0f, static_cast<float>(width - 108) * (g_progress / 100.0f), 8.0f, 4.0f, Color(255, 237, 77, 42));
        DrawRounded(graphics, 54.0f, 448.0f, std::max(8.0f, static_cast<float>(width - 108) * (g_progress / 100.0f)), 3.0f, 2.0f, Color(150, 255, 174, 113));
    }
    DrawText(graphics, L"DRAGON ANTI-CHEAT", 54, 477, 9, Color(255, 104, 104, 111), FontStyleBold);
    DrawText(graphics, L"SECURE INITIALIZATION", static_cast<float>(width - 190), 477.0f, 9.0f, Color(255, 104, 104, 111));
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
            if (g_frames.size() > 1) {
                g_frame = (g_frame + 1) % static_cast<int>(g_frames.size());
                const UINT delay = kSplashFrames[g_frame].duration;
                SetTimer(hwnd, 1, delay < 20 ? 20 : delay, nullptr);
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
    SetTimer(window, 1, g_frames.size() > 1 ? kSplashFrames[0].duration : 45, nullptr);
    SetTimer(window, 2, 65, nullptr);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    for (Image* frame : g_frames) delete frame;
    for (IStream* stream : g_frameStreams) stream->Release();
    delete[] g_fontFamilies;
    GdiplusShutdown(g_gdiplus);
    return static_cast<int>(message.wParam);
}
