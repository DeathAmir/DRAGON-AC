#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <string>
#include <vector>
#include "embedded_assets.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")

using namespace Gdiplus;

static ULONG_PTR g_gdiplus = 0;
static std::vector<Image*> g_frames;
static std::vector<IStream*> g_frameStreams;
static PrivateFontCollection g_fonts;
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
        FontFamily* family = new FontFamily(L"FoRgE", &g_fonts);
        if (family->GetLastStatus() == Ok) g_fontFamily = family;
        else delete family;
    }
}

static void DrawText(Graphics& graphics, const std::wstring& value, float x, float y, float size, Color color, INT style = FontStyleRegular) {
    FontFamily fallback(L"Segoe UI");
    const FontFamily& family = g_fontFamily ? *g_fontFamily : fallback;
    Font font(&family, size, style, UnitPixel);
    SolidBrush brush(color);
    graphics.DrawString(value.c_str(), static_cast<INT>(value.size()), &font, PointF(x, y), &brush);
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
    graphics.Clear(Color(255, 15, 16, 19));

    SolidBrush topLine(Color(255, 220, 76, 48));
    graphics.FillRectangle(&topLine, 0, 0, width, 2);
    Pen border(Color(255, 48, 48, 53), 1.0f);
    graphics.DrawRectangle(&border, 0, 0, width - 1, height - 1);

    const int closeX = width - 36;
    const int closeY = 20;
    Pen closePen(Color(255, 155, 155, 160), 1.5f);
    graphics.DrawLine(&closePen, closeX, closeY, closeX + 10, closeY + 10);
    graphics.DrawLine(&closePen, closeX + 10, closeY, closeX, closeY + 10);

    if (!g_frames.empty()) {
        Image* splash = g_frames[static_cast<size_t>(g_frame) % g_frames.size()];
        const float maxWidth = 132.0f;
        const float maxHeight = 132.0f;
        const float scale = std::min(maxWidth / static_cast<float>(splash->GetWidth()), maxHeight / static_cast<float>(splash->GetHeight()));
        const float drawWidth = splash->GetWidth() * scale;
        const float drawHeight = splash->GetHeight() * scale;
        graphics.DrawImage(splash, RectF(38.0f + (maxWidth - drawWidth) / 2.0f, 106.0f + (maxHeight - drawHeight) / 2.0f, drawWidth, drawHeight));
    }

    DrawText(graphics, L"DRAGON", 205.0f, 137.0f, 39.0f, Color(255, 245, 244, 242), FontStyleBold);
    DrawText(graphics, L"ANTI-CHEAT", 208.0f, 181.0f, 17.0f, Color(255, 225, 83, 55), FontStyleBold);
    DrawText(graphics, L"Starting protection...", 209.0f, 220.0f, 13.0f, Color(255, 145, 146, 151));

    const std::wstring percentage = std::to_wstring(g_progress) + L"%";
    FontFamily fallback(L"Segoe UI");
    const FontFamily& family = g_fontFamily ? *g_fontFamily : fallback;
    Font percentFont(&family, 15.0f, FontStyleBold, UnitPixel);
    SolidBrush percentBrush(Color(255, 231, 91, 57));
    RectF percentRect{};
    graphics.MeasureString(percentage.c_str(), -1, &percentFont, PointF(0, 0), &percentRect);
    graphics.DrawString(percentage.c_str(), -1, &percentFont, PointF(width - 34.0f - percentRect.Width, 270.0f), &percentBrush);

    SolidBrush track(Color(255, 43, 44, 49));
    graphics.FillRectangle(&track, 34, 298, width - 68, 5);
    if (g_progress > 0) {
        SolidBrush fill(Color(255, 229, 78, 49));
        graphics.FillRectangle(&fill, 34, 298, (width - 68) * g_progress / 100, 5);
    }

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
        if (point.y <= 44 && point.x >= client.right - 54) {
            DestroyWindow(hwnd);
            return 0;
        }
        if (point.y <= 58) {
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
            if (point.y <= 44 && point.x >= client.right - 54) {
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
    const int width = 620;
    const int height = 360;
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
    delete g_fontFamily;
    GdiplusShutdown(g_gdiplus);
    return static_cast<int>(message.wParam);
}
