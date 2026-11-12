#include <windows.h>
#include <commctrl.h>
#include <string>
#include <thread>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <cstdarg>
#include <string>
#include <cmath>
#include <mmsystem.h>

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "winmm.lib")

struct RgbColor { int red; int green; int blue; };

struct AppState {
	std::atomic<bool> isRunning{ false };
	std::atomic<bool> hasSaved{ false };
	std::atomic<bool> altWasPressed{ false };
	std::atomic<bool> clicked{ false };
	RgbColor savedColor{ 0, 0, 0 };

	HWND hEditX{ nullptr };
	HWND hEditY{ nullptr };
	HWND hEditRadius{ nullptr };
	HWND hEditTolerance{ nullptr };
	HWND hBtnStart{ nullptr };
	HWND hBtnStop{ nullptr };
	HWND hBtnSave{ nullptr };
	HWND hBtnLoad{ nullptr };
	HWND hInfo{ nullptr };
	HWND hStatus{ nullptr };
	HWND hPreview{ nullptr };

	HANDLE threadHandle{ nullptr };
};

static const int ID_TIMER_PREVIEW = 1;
static const int PREVIEW_MARGIN = 50;

// Forward declarations
static LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK PreviewWndProc(HWND, UINT, WPARAM, LPARAM);
static DWORD WINAPI ClickerThreadProc(LPVOID param);
static void sendLeftClick();
static RgbColor captureAverageColor(int centerX, int centerY, int radius);
static void readIntFromEdit(HWND hEdit, int defaultValue, int &outValue);
static void setEditInt(HWND hEdit, int value);
static void updateInfoLabel(AppState* state);
static void updateStatusLabel(AppState* state, const wchar_t* text, COLORREF color);
static void saveSettings(AppState* state);
static void loadSettings(AppState* state);
static void readAllSettings(AppState* state, int& cx, int& cy, int& radius, int& tol);

// Utility
static void setWindowTextFmt(HWND hwnd, const wchar_t* fmt, ...) {
	wchar_t buf[256];
	va_list args; va_start(args, fmt);
	_vsnwprintf(buf, 255, fmt, args);
	va_end(args);
	SetWindowTextW(hwnd, buf);
}

// Colors comparison
static bool colorsAreDifferent(const RgbColor& a, const RgbColor& b, int tolerancePercent) {
	if (tolerancePercent == 0) {
		return a.red != b.red || a.green != b.green || a.blue != b.blue;
	}
	int tol = (255 * tolerancePercent + 50) / 100;
	if (abs(a.red - b.red) > tol) return true;
	if (abs(a.green - b.green) > tol) return true;
	if (abs(a.blue - b.blue) > tol) return true;
	return false;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
	timeBeginPeriod(1);
	INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES };
	InitCommonControlsEx(&icc);

	const wchar_t* kMainClass = L"AutoClickerGUI_Main";
	const wchar_t* kPreviewClass = L"AutoClickerGUI_Preview";

	WNDCLASSEXW wcex{ sizeof(WNDCLASSEXW) };
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = MainWndProc;
	wcex.hInstance = hInstance;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	wcex.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
	wcex.lpszClassName = kMainClass;
	RegisterClassExW(&wcex);

	WNDCLASSEXW pcx{ sizeof(WNDCLASSEXW) };
	pcx.style = CS_HREDRAW | CS_VREDRAW;
	pcx.lpfnWndProc = PreviewWndProc;
	pcx.hInstance = hInstance;
	pcx.hCursor = LoadCursor(nullptr, IDC_ARROW);
	pcx.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	pcx.lpszClassName = kPreviewClass;
	RegisterClassExW(&pcx);

	AppState* state = new AppState();

	HWND hwnd = CreateWindowExW(
		0, kMainClass, L"Автокликер с предпросмотром (C++)",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, 900, 600, nullptr, nullptr, hInstance, state);
	if (!hwnd) return 0;
	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return static_cast<int>(msg.wParam);
}

static void layoutControls(HWND hwnd, AppState* state) {
	RECT rc; GetClientRect(hwnd, &rc);
	int leftWidth = 400;
	int rightWidth = rc.right - rc.left - leftWidth - 20;
	int x = 10, y = 10, pad = 8;

	// Title
	CreateWindowExW(0, L"STATIC", L"Автокликер с предпросмотром",
		WS_CHILD | WS_VISIBLE, x, y, leftWidth - 20, 24, hwnd, nullptr, nullptr, nullptr);
	y += 30;

	// Coordinates
	CreateWindowExW(0, L"STATIC", L"Координаты центра (X, Y):",
		WS_CHILD | WS_VISIBLE, x, y, 200, 20, hwnd, nullptr, nullptr, nullptr);
	y += 22;

	CreateWindowExW(0, L"STATIC", L"X:", WS_CHILD | WS_VISIBLE, x, y, 18, 20, hwnd, nullptr, nullptr, nullptr);
	state->hEditX = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"640",
		WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL,
		x + 22, y, 80, 22, hwnd, (HMENU)101, nullptr, nullptr);

	CreateWindowExW(0, L"STATIC", L"Y:", WS_CHILD | WS_VISIBLE, x + 120, y, 18, 20, hwnd, nullptr, nullptr, nullptr);
	state->hEditY = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"512",
		WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL,
		x + 142, y, 80, 22, hwnd, (HMENU)102, nullptr, nullptr);
	y += 32;

	// Radius
	CreateWindowExW(0, L"STATIC", L"Радиус области (пиксели):",
		WS_CHILD | WS_VISIBLE, x, y, leftWidth - 20, 20, hwnd, nullptr, nullptr, nullptr);
	y += 22;
	state->hEditRadius = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"2",
		WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL,
		x, y, 80, 22, hwnd, (HMENU)103, nullptr, nullptr);
	y += 32;

	// Tolerance
	CreateWindowExW(0, L"STATIC", L"Погрешность (%):",
		WS_CHILD | WS_VISIBLE, x, y, leftWidth - 20, 20, hwnd, nullptr, nullptr, nullptr);
	y += 22;
	state->hEditTolerance = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"4",
		WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL,
		x, y, 80, 22, hwnd, (HMENU)104, nullptr, nullptr);
	y += 36;

	// Start/Stop buttons
	state->hBtnStart = CreateWindowExW(0, L"BUTTON", L"Запустить",
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		x, y, 120, 28, hwnd, (HMENU)201, nullptr, nullptr);
	state->hBtnStop = CreateWindowExW(0, L"BUTTON", L"Остановить",
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
		x + 130, y, 120, 28, hwnd, (HMENU)202, nullptr, nullptr);
	y += 40;

	// Save/Load
	state->hBtnSave = CreateWindowExW(0, L"BUTTON", L"Сохранить настройки",
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		x, y, 160, 26, hwnd, (HMENU)203, nullptr, nullptr);
	state->hBtnLoad = CreateWindowExW(0, L"BUTTON", L"Загрузить настройки",
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		x + 170, y, 160, 26, hwnd, (HMENU)204, nullptr, nullptr);
	y += 36;

	state->hInfo = CreateWindowExW(0, L"STATIC", L"Нажмите Alt для захвата цвета",
		WS_CHILD | WS_VISIBLE, x, y, leftWidth - 20, 20, hwnd, nullptr, nullptr, nullptr);
	y += 22;

	state->hStatus = CreateWindowExW(0, L"STATIC", L"Статус: Остановлен",
		WS_CHILD | WS_VISIBLE, x, y, leftWidth - 20, 20, hwnd, nullptr, nullptr, nullptr);

	// Preview on right
	state->hPreview = CreateWindowExW(WS_EX_CLIENTEDGE, L"AutoClickerGUI_Preview", L"",
		WS_CHILD | WS_VISIBLE,
		leftWidth, 10, rightWidth, rc.bottom - 20, hwnd, (HMENU)301, nullptr, nullptr);
}

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	static AppState* state = nullptr;
	if (msg == WM_NCCREATE) {
		CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
		state = reinterpret_cast<AppState*>(cs->lpCreateParams);
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
	}
	else {
		state = reinterpret_cast<AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	}

	switch (msg) {
	case WM_CREATE: {
		layoutControls(hwnd, state);
		SetTimer(hwnd, ID_TIMER_PREVIEW, 33, nullptr);
		return 0;
	}
	case WM_SIZE: {
		layoutControls(hwnd, state);
		return 0;
	}
	case WM_TIMER: {
		if (wParam == ID_TIMER_PREVIEW && state && state->hPreview) {
			InvalidateRect(state->hPreview, nullptr, FALSE);
			updateInfoLabel(state);
		}
		return 0;
	}
	case WM_COMMAND: {
		int id = LOWORD(wParam);
		if (id == 201) {
			// Start
			if (!state->isRunning.load()) {
				state->isRunning.store(true);
				EnableWindow(state->hBtnStart, FALSE);
				EnableWindow(state->hBtnStop, TRUE);
				updateStatusLabel(state, L"Статус: Работает", RGB(0, 160, 0));
				HANDLE hThread = CreateThread(nullptr, 0, ClickerThreadProc, state, 0, nullptr);
				state->threadHandle = hThread;
			}
			return 0;
		}
		if (id == 202) {
			// Stop
			state->isRunning.store(false);
			if (state->threadHandle) {
				WaitForSingleObject(state->threadHandle, INFINITE);
				CloseHandle(state->threadHandle);
				state->threadHandle = nullptr;
			}
			state->hasSaved.store(false);
			state->altWasPressed.store(false);
			state->clicked.store(false);
			EnableWindow(state->hBtnStart, TRUE);
			EnableWindow(state->hBtnStop, FALSE);
			updateStatusLabel(state, L"Статус: Остановлен", RGB(200, 120, 0));
			SetWindowTextW(state->hInfo, L"Нажмите Alt для захвата цвета");
			return 0;
		}
		if (id == 203) { saveSettings(state); return 0; }
		if (id == 204) { loadSettings(state); return 0; }
		return 0;
	}
	case WM_DESTROY: {
		KillTimer(hwnd, ID_TIMER_PREVIEW);
		timeEndPeriod(1);
		if (state) {
			state->isRunning.store(false);
			if (state->threadHandle) {
				WaitForSingleObject(state->threadHandle, 2000);
				CloseHandle(state->threadHandle);
			}
			delete state;
		}
		PostQuitMessage(0);
		return 0;
	}
	default:
		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}
}

static void drawPreview(HWND hwnd, HDC hdc) {
	AppState* state = reinterpret_cast<AppState*>(GetWindowLongPtrW(GetParent(hwnd), GWLP_USERDATA));
	if (!state) return;

	RECT rc; GetClientRect(hwnd, &rc);
	int pvW = rc.right - rc.left;
	int pvH = rc.bottom - rc.top;

	int cx = 640, cy = 512, radius = 2;
	readIntFromEdit(state->hEditX, 640, cx);
	readIntFromEdit(state->hEditY, 512, cy);
	readIntFromEdit(state->hEditRadius, 2, radius);

	int left = cx - radius - PREVIEW_MARGIN;
	int top = cy - radius - PREVIEW_MARGIN;
	int regionW = radius * 2 + 1 + PREVIEW_MARGIN * 2;
	int regionH = radius * 2 + 1 + PREVIEW_MARGIN * 2;

	HDC screenDc = GetDC(nullptr);
	SetStretchBltMode(hdc, HALFTONE);
	StretchBlt(hdc, 0, 0, pvW, pvH, screenDc, left, top, regionW, regionH, SRCCOPY);
	ReleaseDC(nullptr, screenDc);

	// Draw red rectangle for the tracked square
	double scaleX = static_cast<double>(pvW) / static_cast<double>(regionW);
	double scaleY = static_cast<double>(pvH) / static_cast<double>(regionH);
	int rectX = static_cast<int>((PREVIEW_MARGIN) * scaleX);
	int rectY = static_cast<int>((PREVIEW_MARGIN) * scaleY);
	int rectW = static_cast<int>((radius * 2 + 1) * scaleX);
	int rectH = static_cast<int>((radius * 2 + 1) * scaleY);

	HPEN pen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
	HGDIOBJ oldPen = SelectObject(hdc, pen);
	HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
	Rectangle(hdc, rectX, rectY, rectX + rectW, rectY + rectH);
	SelectObject(hdc, oldBrush);
	SelectObject(hdc, oldPen);
	DeleteObject(pen);
}

static LRESULT CALLBACK PreviewWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_PAINT: {
		PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
		RECT rc; GetClientRect(hwnd, &rc);
		HBRUSH bg = CreateSolidBrush(RGB(30, 30, 30));
		FillRect(hdc, &rc, bg);
		DeleteObject(bg);
		drawPreview(hwnd, hdc);
		EndPaint(hwnd, &ps);
		return 0;
	}
	default:
		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}
}

static DWORD WINAPI ClickerThreadProc(LPVOID param) {
	AppState* state = reinterpret_cast<AppState*>(param);
	for (;;) {
		if (!state->isRunning.load()) break;
		SHORT s = GetAsyncKeyState(VK_MENU);
		bool altPressed = (s & 0x8000) != 0;

		int cx = 640, cy = 512, radius = 2, tol = 4;
		readIntFromEdit(state->hEditX, 640, cx);
		readIntFromEdit(state->hEditY, 512, cy);
		readIntFromEdit(state->hEditRadius, 2, radius);
		readIntFromEdit(state->hEditTolerance, 4, tol);

		if (altPressed) {
			if (!state->altWasPressed.load()) {
				state->savedColor = captureAverageColor(cx, cy, radius);
				state->hasSaved.store(true);
				state->altWasPressed.store(true);
				state->clicked.store(false);
			} else {
				RgbColor cur = captureAverageColor(cx, cy, radius);
				if (state->hasSaved.load() && colorsAreDifferent(cur, state->savedColor, tol) && !state->clicked.load()) {
					sendLeftClick();
					state->clicked.store(true);
				} else if (state->hasSaved.load() && !colorsAreDifferent(cur, state->savedColor, tol)) {
					state->clicked.store(false);
				}
			}
		} else {
			state->hasSaved.store(false);
			state->altWasPressed.store(false);
			state->clicked.store(false);
		}
		Sleep(0);
	}
	return 0;
}

static void sendLeftClick() {
	INPUT inps[2] = {};
	inps[0].type = INPUT_MOUSE; inps[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
	inps[1].type = INPUT_MOUSE; inps[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
	SendInput(2, inps, sizeof(INPUT));
}

static RgbColor captureAverageColor(int centerX, int centerY, int radius) {
	static HDC screenDc = nullptr;
	static HDC memDc = nullptr;
	static HBITMAP bmp = nullptr;
	static HGDIOBJ oldObj = nullptr;
	static void* bits = nullptr;
	static int cachedW = 0, cachedH = 0;

	int left = centerX - radius;
	int top = centerY - radius;
	int width = radius * 2 + 1;
	int height = radius * 2 + 1;

	if (!screenDc) screenDc = GetDC(nullptr);
	if (!memDc) memDc = CreateCompatibleDC(screenDc);

	if (width != cachedW || height != cachedH || !bmp || !bits) {
		if (memDc && oldObj) { SelectObject(memDc, oldObj); oldObj = nullptr; }
		if (bmp) { DeleteObject(bmp); bmp = nullptr; }
		cachedW = width; cachedH = height;
		BITMAPINFO bmi{};
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = width;
		bmi.bmiHeader.biHeight = -height; // top-down
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;
		bmp = CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
		if (!bmp || !bits) return RgbColor{0,0,0};
		oldObj = SelectObject(memDc, bmp);
	}

	BitBlt(memDc, 0, 0, width, height, screenDc, left, top, SRCCOPY);

	unsigned long long sumR = 0, sumG = 0, sumB = 0;
	std::uint32_t* pixels = static_cast<std::uint32_t*>(bits);
	int total = width * height;
	for (int i = 0; i < total; ++i) {
		std::uint32_t px = pixels[i];
		int b = static_cast<int>(px & 0xFF);
		int g = static_cast<int>((px >> 8) & 0xFF);
		int r = static_cast<int>((px >> 16) & 0xFF);
		sumR += r; sumG += g; sumB += b;
	}

	RgbColor avg{ 0, 0, 0 };
	if (total > 0) {
		avg.red = static_cast<int>(sumR / total);
		avg.green = static_cast<int>(sumG / total);
		avg.blue = static_cast<int>(sumB / total);
	}
	return avg;
}

static void readAllSettings(AppState* state, int& cx, int& cy, int& radius, int& tol) {
	readIntFromEdit(state->hEditX, 640, cx);
	readIntFromEdit(state->hEditY, 512, cy);
	readIntFromEdit(state->hEditRadius, 2, radius);
	readIntFromEdit(state->hEditTolerance, 4, tol);
}

static void updateInfoLabel(AppState* state) {
	if (!state) return;
	if (state->hasSaved.load()) {
		setWindowTextFmt(state->hInfo, L"Захваченный цвет: RGB(%d, %d, %d)",
			state->savedColor.red, state->savedColor.green, state->savedColor.blue);
	} else {
		SetWindowTextW(state->hInfo, L"Нажмите Alt для захвата цвета");
	}
}

static void updateStatusLabel(AppState* state, const wchar_t* text, COLORREF /*color*/) {
	// For simplicity only update text. Coloring statics is non-trivial without custom draw.
	SetWindowTextW(state->hStatus, text);
}

static void readIntFromEdit(HWND hEdit, int defaultValue, int &outValue) {
	wchar_t buf[64]{};
	GetWindowTextW(hEdit, buf, 63);
	if (wcslen(buf) == 0) { outValue = defaultValue; return; }
	outValue = _wtoi(buf);
}

static void setEditInt(HWND hEdit, int value) {
	wchar_t buf[32];
	swprintf(buf, 32, L"%d", value);
	SetWindowTextW(hEdit, buf);
}

// Minimal JSON write/read sufficient for our 4 integer fields
static void saveSettings(AppState* state) {
	int cx=640, cy=512, radius=2, tol=4;
	readAllSettings(state, cx, cy, radius, tol);
	std::wstring json = L"{\"center_x\": " + std::to_wstring(cx)
		+ L", \"center_y\": " + std::to_wstring(cy)
		+ L", \"radius\": " + std::to_wstring(radius)
		+ L", \"tolerance_percent\": " + std::to_wstring(tol) + L"}";

	int len = WideCharToMultiByte(CP_UTF8, 0, json.c_str(), (int)json.size(), nullptr, 0, nullptr, nullptr);
	if (len <= 0) return;
	std::string bytes; bytes.resize((size_t)len);
	WideCharToMultiByte(CP_UTF8, 0, json.c_str(), (int)json.size(), &bytes[0], len, nullptr, nullptr);

	FILE* f = fopen("autoclicker_settings.json", "wb");
	if (!f) return;
	fwrite(bytes.data(), 1, bytes.size(), f);
	fclose(f);
}

static bool extractInt(const std::wstring& s, const wchar_t* key, int& out) {
	size_t p = s.find(key);
	if (p == std::wstring::npos) return false;
	p = s.find(L":", p);
	if (p == std::wstring::npos) return false;
	while (p < s.size() && (s[p] == L':' || iswspace(s[p]))) ++p;
	size_t start = p;
	while (p < s.size() && iswdigit(s[p])) ++p;
	if (p == start) return false;
	out = std::stoi(s.substr(start, p - start));
	return true;
}

static void loadSettings(AppState* state) {
	FILE* f = _wfopen(L"autoclicker_settings.json", L"rb");
	if (!f) return;
	fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
	if (len <= 0 || len > 4096) { fclose(f); return; }
	std::wstring content; content.resize(static_cast<size_t>(len / sizeof(wchar_t)) + len + 4);
	// Read as UTF-8 bytes then convert to wide
	std::string bytes; bytes.resize(static_cast<size_t>(len));
	fread(bytes.data(), 1, bytes.size(), f);
	fclose(f);
	int wlen = MultiByteToWideChar(CP_UTF8, 0, bytes.c_str(), (int)bytes.size(), nullptr, 0);
	if (wlen <= 0) return;
	content.resize(wlen);
	MultiByteToWideChar(CP_UTF8, 0, bytes.c_str(), (int)bytes.size(), &content[0], wlen);

	int cx=640, cy=512, r=2, tol=4;
	extractInt(content, L"center_x", cx);
	extractInt(content, L"center_y", cy);
	extractInt(content, L"radius", r);
	extractInt(content, L"tolerance_percent", tol);

	setEditInt(state->hEditX, cx);
	setEditInt(state->hEditY, cy);
	setEditInt(state->hEditRadius, r);
	setEditInt(state->hEditTolerance, tol);
}

