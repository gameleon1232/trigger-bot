#include <windows.h>
#include <vector>
#include <cstdint>
#include <iostream>
#include <thread>
#include <atomic>
#include <cmath>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

struct DibSectionCapture {
	HDC screenDc{ nullptr };
	HDC memDc{ nullptr };
	HBITMAP bitmap{ nullptr };
	void* bits{ nullptr };
	int width{ 0 };
	int height{ 0 };
	HGDIOBJ oldObj{ nullptr };

	bool initialize(int w, int h) {
		cleanup();
		width = w; height = h;
		screenDc = GetDC(nullptr);
		memDc = CreateCompatibleDC(screenDc);
		BITMAPINFO bmi{};
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = width;
		bmi.bmiHeader.biHeight = -height;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;
		bitmap = CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
		if (!bitmap || !bits) { cleanup(); return false; }
		oldObj = SelectObject(memDc, bitmap);
		return true;
	}

	void capture(int left, int top) {
		BitBlt(memDc, 0, 0, width, height, screenDc, left, top, SRCCOPY);
	}

	std::uint32_t* pixelData() const { return static_cast<std::uint32_t*>(bits); }

	void cleanup() {
		if (memDc && oldObj) { SelectObject(memDc, oldObj); oldObj = nullptr; }
		if (bitmap) { DeleteObject(bitmap); bitmap = nullptr; }
		if (memDc) { DeleteDC(memDc); memDc = nullptr; }
		if (screenDc) { ReleaseDC(nullptr, screenDc); screenDc = nullptr; }
		bits = nullptr; width = 0; height = 0;
	}

	~DibSectionCapture() { cleanup(); }
};

struct RgbColor {
	int red;
	int green;
	int blue;
};

static RgbColor captureAverageColor(DibSectionCapture& cap, int centerX, int centerY, int radius) {
	int left = centerX - radius;
	int top = centerY - radius;
	int width = radius * 2 + 1;
	int height = radius * 2 + 1;
	if (cap.width != width || cap.height != height) {
		cap.initialize(width, height);
	}
	cap.capture(left, top);

	unsigned long long sumR = 0, sumG = 0, sumB = 0;
	std::uint32_t* pixels = cap.pixelData();
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

static bool colorsAreDifferent(const RgbColor& a, const RgbColor& b, int tolerancePercent) {
	if (tolerancePercent == 0) {
		return a.red != b.red || a.green != b.green || a.blue != b.blue;
	}
	const int tolerance = (255 * tolerancePercent + 50) / 100;
	if (abs(a.red - b.red) > tolerance) return true;
	if (abs(a.green - b.green) > tolerance) return true;
	if (abs(a.blue - b.blue) > tolerance) return true;
	return false;
}

static void sendLeftClick() {
	INPUT inputs[2] = {};
	inputs[0].type = INPUT_MOUSE;
	inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
	inputs[1].type = INPUT_MOUSE;
	inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
	SendInput(2, inputs, sizeof(INPUT));
}

int main() {
	// Defaults aligned with Python version
	const int CENTER_X = 640;
	const int CENTER_Y = 512;
	const int RADIUS = 2;
	const int TOLERANCE_PERCENT = 4;

	std::cout << "Зажмите левый Alt, чтобы запомнить средний цвет в центре экрана (радиус "
			  << RADIUS << ")." << std::endl;
	std::cout << "Погрешность: " << TOLERANCE_PERCENT
			  << "%. Если цвет изменится больше чем на " << TOLERANCE_PERCENT
			  << "% — будет произведён клик." << std::endl;

	timeBeginPeriod(1);
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	bool altWasPressed = false;
	bool clicked = false;
	bool hasSaved = false;
	RgbColor saved{ 0, 0, 0 };
	DibSectionCapture cap;

	for (;;) {
		SHORT state = GetAsyncKeyState(VK_MENU);
		bool altPressed = (state & 0x8000) != 0;

		if (altPressed) {
			if (!altWasPressed) {
				saved = captureAverageColor(cap, CENTER_X, CENTER_Y, RADIUS);
				hasSaved = true;
				altWasPressed = true;
				clicked = false;
				std::cout << "Средний цвет запомнен: RGB("
					  << saved.red << ", " << saved.green << ", " << saved.blue << ")" << std::endl;
			} else {
				RgbColor current = captureAverageColor(cap, CENTER_X, CENTER_Y, RADIUS);
				if (hasSaved && colorsAreDifferent(current, saved, TOLERANCE_PERCENT) && !clicked) {
					sendLeftClick();
					std::cout << "Средний цвет изменился! Клик!" << std::endl;
					clicked = true;
				} else if (hasSaved && !colorsAreDifferent(current, saved, TOLERANCE_PERCENT)) {
					clicked = false;
				}
			}
		} else {
			hasSaved = false;
			altWasPressed = false;
			clicked = false;
		}

		Sleep(0);
	}

	return 0;
}

