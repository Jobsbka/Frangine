#!/usr/bin/env python3
"""
Frabgine Engine Build & Test GUI
Простой интерфейс для компиляции и тестирования движка
"""

import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox, filedialog
import subprocess
import threading
import os
import sys

class FrabgineBuilderGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Frabgine Engine - Build & Test")
        self.root.geometry("900x700")
        
        # Пути
        self.engine_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "frabgine_engine")
        self.build_path = os.path.join(self.engine_path, "build")
        
        # Переменные
        self.build_type = tk.StringVar(value="Release")
        self.enable_vulkan = tk.BooleanVar(value=True)
        self.build_editor = tk.BooleanVar(value=True)
        self.process = None
        
        self.create_widgets()
        self.check_dependencies()
        
    def create_widgets(self):
        """Создание элементов интерфейса"""
        # Верхняя панель с настройками
        settings_frame = ttk.LabelFrame(self.root, text="Настройки сборки", padding=10)
        settings_frame.pack(fill="x", padx=10, pady=5)
        
        # Путь к проекту
        path_frame = ttk.Frame(settings_frame)
        path_frame.pack(fill="x", pady=5)
        
        ttk.Label(path_frame, text="Путь к проекту:").pack(side="left")
        self.path_entry = ttk.Entry(path_frame, width=60)
        self.path_entry.pack(side="left", padx=5)
        self.path_entry.insert(0, self.engine_path)
        
        ttk.Button(path_frame, text="Обзор...", command=self.browse_path).pack(side="left")
        
        # Тип сборки
        type_frame = ttk.Frame(settings_frame)
        type_frame.pack(fill="x", pady=5)
        
        ttk.Label(type_frame, text="Тип сборки:").pack(side="left")
        ttk.Radiobutton(type_frame, text="Debug", variable=self.build_type, value="Debug").pack(side="left", padx=5)
        ttk.Radiobutton(type_frame, text="Release", variable=self.build_type, value="Release").pack(side="left", padx=5)
        ttk.Radiobutton(type_frame, text="RelWithDebInfo", variable=self.build_type, value="RelWithDebInfo").pack(side="left", padx=5)
        
        # Опции
        options_frame = ttk.Frame(settings_frame)
        options_frame.pack(fill="x", pady=5)
        
        ttk.Checkbutton(options_frame, text="Включить Vulkan", variable=self.enable_vulkan).pack(side="left", padx=10)
        ttk.Checkbutton(options_frame, text="Собрать редактор", variable=self.build_editor).pack(side="left", padx=10)
        
        # Кнопки управления
        btn_frame = ttk.Frame(self.root, padding=10)
        btn_frame.pack(fill="x", padx=10, pady=5)
        
        self.configure_btn = ttk.Button(btn_frame, text="1. Configure (CMake)", command=self.configure_cmake)
        self.configure_btn.pack(side="left", padx=5)
        
        self.build_btn = ttk.Button(btn_frame, text="2. Build", command=self.build_project)
        self.build_btn.pack(side="left", padx=5)
        
        self.clean_btn = ttk.Button(btn_frame, text="Clean", command=self.clean_build)
        self.clean_btn.pack(side="left", padx=5)
        
        self.run_btn = ttk.Button(btn_frame, text="3. Run Tests", command=self.run_tests)
        self.run_btn.pack(side="left", padx=5)
        
        self.stop_btn = ttk.Button(btn_frame, text="Stop", command=self.stop_process, state="disabled")
        self.stop_btn.pack(side="right", padx=5)
        
        # Статус бар
        status_frame = ttk.Frame(self.root)
        status_frame.pack(fill="x", padx=10, pady=5)
        
        self.status_label = ttk.Label(status_frame, text="Готов к работе", foreground="green")
        self.status_label.pack(side="left")
        
        self.progress = ttk.Progressbar(status_frame, mode="indeterminate")
        self.progress.pack(side="right", fill="x", expand=True, padx=10)
        
        # Окно вывода
        output_frame = ttk.LabelFrame(self.root, text="Вывод консоли", padding=10)
        output_frame.pack(fill="both", expand=True, padx=10, pady=5)
        
        self.output_text = scrolledtext.ScrolledText(output_frame, wrap=tk.WORD, height=20)
        self.output_text.pack(fill="both", expand=True)
        
        # Нижняя панель с информацией
        info_frame = ttk.LabelFrame(self.root, text="Информация о системе", padding=10)
        info_frame.pack(fill="x", padx=10, pady=5)
        
        self.info_text = tk.Text(info_frame, height=6, wrap=tk.WORD)
        self.info_text.pack(fill="x")
        self.update_system_info()
        
    def browse_path(self):
        """Выбор пути к проекту"""
        path = filedialog.askdirectory(initialdir=self.engine_path)
        if path:
            self.path_entry.delete(0, tk.END)
            self.path_entry.insert(0, path)
            self.engine_path = path
            
    def update_system_info(self):
        """Обновление информации о системе"""
        self.info_text.delete(1.0, tk.END)
        
        try:
            # CMake версия
            cmake_ver = subprocess.check_output(["cmake", "--version"], text=True).split('\n')[0]
            
            # G++ версия
            gpp_ver = subprocess.check_output(["g++", "--version"], text=True).split('\n')[0]
            
            # Python версия
            py_ver = f"Python {sys.version.split()[0]}"
            
            # OS
            os_info = os.name
            
            info = f"{cmake_ver}\n{gpp_ver}\n{py_ver}\nOS: {os_info}"
            self.info_text.insert(tk.END, info)
        except Exception as e:
            self.info_text.insert(tk.END, f"Ошибка получения информации: {e}")
            
    def check_dependencies(self):
        """Проверка зависимостей"""
        deps = {
            "cmake": "cmake --version",
            "g++": "g++ --version",
            "python": "python3 --version"
        }
        
        missing = []
        for name, cmd in deps.items():
            try:
                subprocess.check_output(cmd.split(), stderr=subprocess.DEVNULL)
            except:
                missing.append(name)
                
        if missing:
            messagebox.showwarning("Предупреждение", 
                f"Отсутствуют зависимости: {', '.join(missing)}\nСборка может не работать")
                
    def log(self, message, color="black"):
        """Добавление сообщения в лог"""
        self.output_text.insert(tk.END, message + "\n")
        self.output_text.tag_add(color, "end-2c", "end-1c")
        self.output_text.tag_config(color, foreground=color)
        self.output_text.see(tk.END)
        
    def set_status(self, status, color="green"):
        """Установка статуса"""
        self.status_label.config(text=status, foreground=color)
        
    def toggle_buttons(self, running):
        """Переключение состояния кнопок"""
        state = "disabled" if running else "normal"
        self.configure_btn.config(state=state)
        self.build_btn.config(state=state)
        self.clean_btn.config(state=state)
        self.run_btn.config(state=state)
        self.stop_btn.config(state="normal" if running else "disabled")
        
        if running:
            self.progress.start()
        else:
            self.progress.stop()
            
    def run_command(self, cmd, cwd=None):
        """Выполнение команды в отдельном потоке"""
        def thread_func():
            try:
                self.process = subprocess.Popen(
                    cmd,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True,
                    cwd=cwd or self.engine_path,
                    shell=isinstance(cmd, str)
                )
                
                for line in iter(self.process.stdout.readline, ''):
                    if line:
                        self.root.after(0, lambda l=line: self.log(l.rstrip()))
                        
                self.process.wait()
                
                if self.process.returncode == 0:
                    self.root.after(0, lambda: self.set_status("Успешно", "green"))
                else:
                    self.root.after(0, lambda: self.set_status(f"Ошибка (код {self.process.returncode})", "red"))
                    
            except Exception as e:
                self.root.after(0, lambda: self.log(f"Ошибка: {e}", "red"))
                self.root.after(0, lambda: self.set_status("Ошибка выполнения", "red"))
            finally:
                self.process = None
                self.root.after(0, lambda: self.toggle_buttons(False))
                
        self.toggle_buttons(True)
        self.output_text.delete(1.0, tk.END)
        threading.Thread(target=thread_func, daemon=True).start()
        
    def configure_cmake(self):
        """Настройка CMake"""
        self.engine_path = self.path_entry.get()
        self.build_path = os.path.join(self.engine_path, "build")
        
        if not os.path.exists(self.engine_path):
            messagebox.showerror("Ошибка", f"Путь не существует: {self.engine_path}")
            return
            
        os.makedirs(self.build_path, exist_ok=True)
        
        cmd = [
            "cmake",
            f"-DCMAKE_BUILD_TYPE={self.build_type.get()}",
            f"-DFRABGINE_ENABLE_VULKAN={'ON' if self.enable_vulkan.get() else 'OFF'}",
            f"-DFRABGINE_BUILD_EDITOR={'ON' if self.build_editor.get() else 'OFF'}",
            "-S", self.engine_path,
            "-B", self.build_path
        ]
        
        self.log("=" * 60)
        self.log(f"Конфигурация CMake: {' '.join(cmd)}")
        self.log("=" * 60)
        
        self.run_command(cmd)
        
    def build_project(self):
        """Сборка проекта"""
        if not os.path.exists(self.build_path):
            messagebox.showerror("Ошибка", "Сначала выполните Configure!")
            return
            
        cmd = ["cmake", "--build", self.build_path, "--config", self.build_type.get()]
        
        self.log("=" * 60)
        self.log(f"Сборка проекта: {' '.join(cmd)}")
        self.log("=" * 60)
        
        self.run_command(cmd)
        
    def clean_build(self):
        """Очистка сборки"""
        if os.path.exists(self.build_path):
            if messagebox.askyesno("Подтверждение", "Удалить папку build?"):
                import shutil
                try:
                    shutil.rmtree(self.build_path)
                    self.log("Папка build удалена")
                    self.set_status("Очищено", "blue")
                except Exception as e:
                    self.log(f"Ошибка очистки: {e}", "red")
        else:
            self.log("Папка build не существует")
            
    def run_tests(self):
        """Запуск тестов"""
        test_executable = os.path.join(self.build_path, "frabgine_tests")
        
        if not os.path.exists(test_executable):
            # Пробуем найти исполняемый файл редактора как альтернативу
            editor_executable = os.path.join(self.build_path, "frabgine_editor")
            if os.path.exists(editor_executable):
                self.log("Тесты не найдены,尝试 запустить редактор...")
                self.run_command([editor_executable])
            else:
                messagebox.showerror("Ошибка", 
                    "Исполняемый файл не найден.\nСначала выполните сборку проекта.")
            return
            
        self.log("=" * 60)
        self.log("Запуск тестов...")
        self.log("=" * 60)
        
        self.run_command([test_executable])
        
    def stop_process(self):
        """Остановка текущего процесса"""
        if self.process:
            self.process.terminate()
            self.log("Процесс остановлен пользователем", "red")
            self.set_status("Остановлено", "orange")
            self.toggle_buttons(False)


def main():
    root = tk.Tk()
    
    # Установка стиля
    style = ttk.Style()
    style.theme_use('clam')
    
    # Настройка шрифтов
    default_font = ("Arial", 10)
    root.option_add("*Font", default_font)
    
    app = FrabgineBuilderGUI(root)
    root.mainloop()


if __name__ == "__main__":
    main()
