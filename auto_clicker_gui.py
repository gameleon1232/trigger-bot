import tkinter as tk
from tkinter import ttk, messagebox
import threading
import time
from pynput.mouse import Button, Controller as MouseController
from PIL import ImageGrab, Image, ImageTk
import keyboard
import numpy as np
import json
import os

class AutoClickerGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Автокликер с предпросмотром")
        self.root.geometry("800x600")
        self.root.configure(bg='#2b2b2b')
        
        # Переменные
        self.center_x = tk.IntVar(value=640)
        self.center_y = tk.IntVar(value=512)
        self.radius = tk.IntVar(value=2)
        self.tolerance_percent = tk.IntVar(value=4)
        self.is_running = False
        self.saved_color = None
        self.alt_was_pressed = False
        self.clicked = False
        
        # Контроллеры
        self.mouse = MouseController()
        self.thread = None
        
        # Загрузка настроек
        self.load_settings()
        
        # Создание интерфейса
        self.create_widgets()
        
        # Запуск обновления предпросмотра
        self.update_preview()
        
    def create_widgets(self):
        # Главный контейнер
        main_frame = tk.Frame(self.root, bg='#2b2b2b')
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Заголовок
        title_label = tk.Label(
            main_frame, 
            text="Автокликер с предпросмотром", 
            font=('Arial', 16, 'bold'),
            fg='#ffffff',
            bg='#2b2b2b'
        )
        title_label.pack(pady=(0, 20))
        
        # Контейнер для настроек и предпросмотра
        content_frame = tk.Frame(main_frame, bg='#2b2b2b')
        content_frame.pack(fill=tk.BOTH, expand=True)
        
        # Левая панель - настройки
        settings_frame = tk.LabelFrame(
            content_frame,
            text="Настройки",
            font=('Arial', 12, 'bold'),
            fg='#ffffff',
            bg='#2b2b2b',
            relief=tk.RAISED,
            bd=2
        )
        settings_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(0, 10))
        
        # Координаты
        coords_frame = tk.Frame(settings_frame, bg='#2b2b2b')
        coords_frame.pack(fill=tk.X, padx=10, pady=10)
        
        tk.Label(coords_frame, text="Координаты центра:", font=('Arial', 10), fg='#ffffff', bg='#2b2b2b').pack(anchor=tk.W)
        
        coord_frame = tk.Frame(coords_frame, bg='#2b2b2b')
        coord_frame.pack(fill=tk.X, pady=5)
        
        tk.Label(coord_frame, text="X:", font=('Arial', 10), fg='#ffffff', bg='#2b2b2b').pack(side=tk.LEFT)
        tk.Entry(coord_frame, textvariable=self.center_x, width=8, font=('Arial', 10)).pack(side=tk.LEFT, padx=(5, 10))
        
        tk.Label(coord_frame, text="Y:", font=('Arial', 10), fg='#ffffff', bg='#2b2b2b').pack(side=tk.LEFT)
        tk.Entry(coord_frame, textvariable=self.center_y, width=8, font=('Arial', 10)).pack(side=tk.LEFT, padx=5)
        
        # Радиус
        radius_frame = tk.Frame(settings_frame, bg='#2b2b2b')
        radius_frame.pack(fill=tk.X, padx=10, pady=10)
        
        tk.Label(radius_frame, text="Радиус области (пиксели):", font=('Arial', 10), fg='#ffffff', bg='#2b2b2b').pack(anchor=tk.W)
        radius_scale = tk.Scale(
            radius_frame,
            from_=1,
            to=20,
            orient=tk.HORIZONTAL,
            variable=self.radius,
            bg='#2b2b2b',
            fg='#ffffff',
            highlightbackground='#2b2b2b',
            troughcolor='#404040',
            activebackground='#404040'
        )
        radius_scale.pack(fill=tk.X, pady=5)
        
        # Погрешность
        tolerance_frame = tk.Frame(settings_frame, bg='#2b2b2b')
        tolerance_frame.pack(fill=tk.X, padx=10, pady=10)
        
        tk.Label(tolerance_frame, text="Погрешность (%):", font=('Arial', 10), fg='#ffffff', bg='#2b2b2b').pack(anchor=tk.W)
        tolerance_scale = tk.Scale(
            tolerance_frame,
            from_=0,
            to=50,
            orient=tk.HORIZONTAL,
            variable=self.tolerance_percent,
            bg='#2b2b2b',
            fg='#ffffff',
            highlightbackground='#2b2b2b',
            troughcolor='#404040',
            activebackground='#404040'
        )
        tolerance_scale.pack(fill=tk.X, pady=5)
        
        # Кнопки управления
        buttons_frame = tk.Frame(settings_frame, bg='#2b2b2b')
        buttons_frame.pack(fill=tk.X, padx=10, pady=20)
        
        self.start_button = tk.Button(
            buttons_frame,
            text="Запустить",
            command=self.start_clicker,
            bg='#4CAF50',
            fg='white',
            font=('Arial', 10, 'bold'),
            relief=tk.FLAT,
            padx=20,
            pady=5
        )
        self.start_button.pack(side=tk.LEFT, padx=(0, 10))
        
        self.stop_button = tk.Button(
            buttons_frame,
            text="Остановить",
            command=self.stop_clicker,
            bg='#f44336',
            fg='white',
            font=('Arial', 10, 'bold'),
            relief=tk.FLAT,
            padx=20,
            pady=5,
            state=tk.DISABLED
        )
        self.stop_button.pack(side=tk.LEFT)
        
        # Кнопки сохранения/загрузки
        save_load_frame = tk.Frame(settings_frame, bg='#2b2b2b')
        save_load_frame.pack(fill=tk.X, padx=10, pady=10)
        
        tk.Button(
            save_load_frame,
            text="Сохранить настройки",
            command=self.save_settings,
            bg='#2196F3',
            fg='white',
            font=('Arial', 9),
            relief=tk.FLAT,
            padx=15,
            pady=3
        ).pack(side=tk.LEFT, padx=(0, 5))
        
        tk.Button(
            save_load_frame,
            text="Загрузить настройки",
            command=self.load_settings,
            bg='#FF9800',
            fg='white',
            font=('Arial', 9),
            relief=tk.FLAT,
            padx=15,
            pady=3
        ).pack(side=tk.LEFT)
        
        # Правая панель - предпросмотр
        preview_frame = tk.LabelFrame(
            content_frame,
            text="Предпросмотр области захвата",
            font=('Arial', 12, 'bold'),
            fg='#ffffff',
            bg='#2b2b2b',
            relief=tk.RAISED,
            bd=2
        )
        preview_frame.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=(10, 0))
        
        # Канвас для предпросмотра
        self.canvas = tk.Canvas(
            preview_frame,
            bg='#1e1e1e',
            highlightthickness=0
        )
        self.canvas.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Информационная панель
        info_frame = tk.Frame(preview_frame, bg='#2b2b2b')
        info_frame.pack(fill=tk.X, padx=10, pady=(0, 10))
        
        self.info_label = tk.Label(
            info_frame,
            text="Нажмите Alt для захвата цвета",
            font=('Arial', 10),
            fg='#ffffff',
            bg='#2b2b2b'
        )
        self.info_label.pack()
        
        # Статус
        self.status_label = tk.Label(
            info_frame,
            text="Статус: Остановлен",
            font=('Arial', 10),
            fg='#ff9800',
            bg='#2b2b2b'
        )
        self.status_label.pack()
        
    def get_average_color(self, img, x, y, radius):
        try:
            left = max(0, x - radius)
            top = max(0, y - radius)
            right = min(img.width, x + radius + 1)
            bottom = min(img.height, y + radius + 1)
            region = img.crop((left, top, right, bottom))
            arr = np.array(region)
            avg = arr.mean(axis=(0, 1))
            return tuple(int(v) for v in avg)
        except:
            return (0, 0, 0)
    
    def colors_are_different(self, color1, color2, tolerance_percent):
        if tolerance_percent == 0:
            return color1 != color2
        
        for i in range(3):  # RGB каналы
            max_value = 255
            tolerance = max_value * tolerance_percent / 100
            if abs(color1[i] - color2[i]) > tolerance:
                return True
        return False
    
    def update_preview(self):
        try:
            # Захват экрана
            img = ImageGrab.grab()
            
            # Создание уменьшенного изображения для предпросмотра
            preview_width = 300
            preview_height = 200
            
            # Вычисление области вокруг центра
            x, y = self.center_x.get(), self.center_y.get()
            radius = self.radius.get()
            
            left = max(0, x - radius - 50)
            top = max(0, y - radius - 50)
            right = min(img.width, x + radius + 50)
            bottom = min(img.height, y + radius + 50)
            
            # Обрезка области
            region = img.crop((left, top, right, bottom))
            
            # Изменение размера для предпросмотра
            region.thumbnail((preview_width, preview_height), Image.Resampling.LANCZOS)
            
            # Конвертация для tkinter
            photo = ImageTk.PhotoImage(region)
            
            # Очистка канваса
            self.canvas.delete("all")
            
            # Отображение изображения
            self.canvas.create_image(
                preview_width // 2,
                preview_height // 2,
                image=photo,
                anchor=tk.CENTER
            )
            
            # Рисование рамки области захвата
            center_x_preview = (x - left) * preview_width // (right - left)
            center_y_preview = (y - top) * preview_height // (bottom - top)
            radius_preview = radius * preview_width // (right - left)
            
            self.canvas.create_rectangle(
                center_x_preview - radius_preview,
                center_y_preview - radius_preview,
                center_x_preview + radius_preview,
                center_y_preview + radius_preview,
                outline='#ff0000',
                width=2
            )
            
            # Сохранение ссылки на изображение
            self.canvas.image = photo
            
            # Обновление информации
            if self.saved_color:
                self.info_label.config(text=f"Захваченный цвет: RGB{self.saved_color}")
            else:
                self.info_label.config(text="Нажмите Alt для захвата цвета")
                
        except Exception as e:
            print(f"Ошибка обновления предпросмотра: {e}")
        
        # Повтор через 100мс
        self.root.after(100, self.update_preview)
    
    def start_clicker(self):
        if not self.is_running:
            self.is_running = True
            self.start_button.config(state=tk.DISABLED)
            self.stop_button.config(state=tk.NORMAL)
            self.status_label.config(text="Статус: Работает", fg='#4CAF50')
            
            # Запуск в отдельном потоке
            self.thread = threading.Thread(target=self.clicker_loop, daemon=True)
            self.thread.start()
    
    def stop_clicker(self):
        self.is_running = False
        self.saved_color = None
        self.alt_was_pressed = False
        self.clicked = False
        
        self.start_button.config(state=tk.NORMAL)
        self.stop_button.config(state=tk.DISABLED)
        self.status_label.config(text="Статус: Остановлен", fg='#ff9800')
        self.info_label.config(text="Нажмите Alt для захвата цвета")
    
    def clicker_loop(self):
        while self.is_running:
            try:
                if keyboard.is_pressed('alt'):
                    if not self.alt_was_pressed:
                        img = ImageGrab.grab()
                        self.saved_color = self.get_average_color(
                            img, 
                            self.center_x.get(), 
                            self.center_y.get(), 
                            self.radius.get()
                        )
                        self.alt_was_pressed = True
                        self.clicked = False
                        print(f"Средний цвет запомнен: {self.saved_color}")
                    else:
                        img = ImageGrab.grab()
                        current_color = self.get_average_color(
                            img, 
                            self.center_x.get(), 
                            self.center_y.get(), 
                            self.radius.get()
                        )
                        if self.colors_are_different(current_color, self.saved_color, self.tolerance_percent.get()) and not self.clicked:
                            self.mouse.press(Button.left)
                            self.mouse.release(Button.left)
                            print("Средний цвет изменился! Клик!")
                            self.clicked = True
                        elif not self.colors_are_different(current_color, self.saved_color, self.tolerance_percent.get()):
                            self.clicked = False
                else:
                    self.saved_color = None
                    self.alt_was_pressed = False
                    self.clicked = False
                    
                time.sleep(0.001)
            except Exception as e:
                print(f"Ошибка в цикле кликера: {e}")
                time.sleep(0.1)
    
    def save_settings(self):
        settings = {
            'center_x': self.center_x.get(),
            'center_y': self.center_y.get(),
            'radius': self.radius.get(),
            'tolerance_percent': self.tolerance_percent.get()
        }
        
        try:
            with open('autoclicker_settings.json', 'w') as f:
                json.dump(settings, f)
            messagebox.showinfo("Успех", "Настройки сохранены!")
        except Exception as e:
            messagebox.showerror("Ошибка", f"Не удалось сохранить настройки: {e}")
    
    def load_settings(self):
        try:
            if os.path.exists('autoclicker_settings.json'):
                with open('autoclicker_settings.json', 'r') as f:
                    settings = json.load(f)
                
                self.center_x.set(settings.get('center_x', 640))
                self.center_y.set(settings.get('center_y', 512))
                self.radius.set(settings.get('radius', 2))
                self.tolerance_percent.set(settings.get('tolerance_percent', 4))
                
                messagebox.showinfo("Успех", "Настройки загружены!")
            else:
                messagebox.showinfo("Информация", "Файл настроек не найден")
        except Exception as e:
            messagebox.showerror("Ошибка", f"Не удалось загрузить настройки: {e}")

def main():
    root = tk.Tk()
    app = AutoClickerGUI(root)
    root.mainloop()

if __name__ == "__main__":
    main() 