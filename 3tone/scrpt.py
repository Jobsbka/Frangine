#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Скрипт для построения структуры папок (и файлов) в виде дерева.
Сохраняет результат в текстовый файл.
"""

import os
import sys
import argparse
from pathlib import Path

def write_tree_structure(start_path, output_file, include_files=True, max_depth=None, prefix=""):
    """
    Рекурсивно записывает структуру каталога в файл.
    
    Args:
        start_path: путь к корневому каталогу
        output_file: файловый объект для записи
        include_files: включать ли файлы в вывод
        max_depth: максимальная глубина обхода (None - без ограничений)
        prefix: префикс для текущего уровня (используется в рекурсии)
    """
    if max_depth is not None and max_depth <= 0:
        return
    
    try:
        # Получаем содержимое каталога и сортируем
        entries = sorted(os.listdir(start_path))
    except PermissionError:
        output_file.write(f"{prefix}[Нет доступа]\n")
        return
    except NotADirectoryError:
        return
    except Exception as e:
        output_file.write(f"{prefix}[Ошибка: {e}]\n")
        return

    # Отделяем папки от файлов для правильного отображения дерева
    dirs = []
    files = []
    for entry in entries:
        full_path = os.path.join(start_path, entry)
        try:
            if os.path.isdir(full_path):
                dirs.append(entry)
            else:
                files.append(entry)
        except PermissionError:
            # Если не удаётся определить тип, считаем файлом (или просто игнорируем)
            files.append(entry)
    
    # Сначала выводим папки
    for i, name in enumerate(dirs):
        is_last = (i == len(dirs) - 1 and not (include_files and files))
        connector = "└── " if is_last else "├── "
        output_file.write(f"{prefix}{connector}{name}/\n")
        
        # Рекурсивный вызов для подпапки
        new_prefix = prefix + ("    " if is_last else "│   ")
        sub_path = os.path.join(start_path, name)
        write_tree_structure(sub_path, output_file, include_files,
                             None if max_depth is None else max_depth - 1,
                             new_prefix)
    
    # Затем выводим файлы (если требуется)
    if include_files:
        for i, name in enumerate(files):
            is_last = (i == len(files) - 1)
            connector = "└── " if is_last else "├── "
            output_file.write(f"{prefix}{connector}{name}\n")


def main():
    parser = argparse.ArgumentParser(
        description="Обход корневого каталога и создание txt-файла со структурой папок и файлов"
    )
    parser.add_argument(
        "--path", "-p",
        type=str,
        default=".",
        help="Путь к корневому каталогу (по умолчанию: текущий каталог)"
    )
    parser.add_argument(
        "--output", "-o",
        type=str,
        default="folder_structure.txt",
        help="Имя выходного txt-файла (по умолчанию: folder_structure.txt)"
    )
    parser.add_argument(
        "--no-files", "-nf",
        action="store_true",
        help="Не включать файлы в вывод, только папки"
    )
    parser.add_argument(
        "--max-depth", "-d",
        type=int,
        default=None,
        help="Максимальная глубина обхода (по умолчанию: без ограничений)"
    )
    
    args = parser.parse_args()
    
    # Преобразуем путь в абсолютный для удобства
    root_path = os.path.abspath(args.path)
    
    if not os.path.exists(root_path):
        print(f"Ошибка: путь '{root_path}' не существует.")
        sys.exit(1)
    
    if not os.path.isdir(root_path):
        print(f"Ошибка: '{root_path}' не является каталогом.")
        sys.exit(1)
    
    # Запись в файл
    try:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(f"Структура каталога: {root_path}\n")
            f.write("=" * 60 + "\n\n")
            write_tree_structure(
                root_path, f,
                include_files=not args.no_files,
                max_depth=args.max_depth
            )
        print(f"Структура успешно сохранена в файл: {args.output}")
    except Exception as e:
        print(f"Ошибка при записи файла: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()