import time
from pynput.mouse import Button, Controller as MouseController
from PIL import ImageGrab
import keyboard
import numpy as np

# Центр экрана для 1280x1024
CENTER_X, CENTER_Y = 640, 512
RADIUS = 2  # радиус в пикселях (от центра в стороны)
TOLERANCE_PERCENT = 4  # процент погрешности (0-100)

mouse = MouseController()
saved_color = None
alt_was_pressed = False
clicked = False

def get_average_color(img, x, y, radius):
    left = x - radius
    top = y - radius
    right = x + radius + 1
    bottom = y + radius + 1
    region = img.crop((left, top, right, bottom))
    arr = np.array(region)
    avg = arr.mean(axis=(0, 1))
    return tuple(int(v) for v in avg)

def colors_are_different(color1, color2, tolerance_percent):
    if tolerance_percent == 0:
        return color1 != color2
    
    for i in range(3):  # RGB каналы
        max_value = 255
        tolerance = max_value * tolerance_percent / 100
        if abs(color1[i] - color2[i]) > tolerance:
            return True
    return False

print(f"Зажмите левый Alt, чтобы запомнить средний цвет в центре экрана (радиус {RADIUS}).")
print(f"Погрешность: {TOLERANCE_PERCENT}%. Если цвет изменится больше чем на {TOLERANCE_PERCENT}% — будет произведён клик.")

while True:
    if keyboard.is_pressed('alt'):
        if not alt_was_pressed:
            img = ImageGrab.grab()
            saved_color = get_average_color(img, CENTER_X, CENTER_Y, RADIUS)
            alt_was_pressed = True
            clicked = False
            print(f"Средний цвет запомнен: {saved_color}")
        else:
            img = ImageGrab.grab()
            current_color = get_average_color(img, CENTER_X, CENTER_Y, RADIUS)
            if colors_are_different(current_color, saved_color, TOLERANCE_PERCENT) and not clicked:
                mouse.press(Button.left)
                mouse.release(Button.left)
                print("Средний цвет изменился! Клик!")
                clicked = True
            elif not colors_are_different(current_color, saved_color, TOLERANCE_PERCENT):
                clicked = False
    else:
        saved_color = None
        alt_was_pressed = False
        clicked = False
    time.sleep(0.001)  # уменьшаем задержку для мгновенной реакции