#!/usr/bin/env python3
"""
Скрипт для построения дерева файлов и папок.
Использование: python tree_structure.py [путь_к_каталогу]
Если путь не указан, используется текущий каталог.
Результат сохраняется в файл structure.txt.
"""

import sys
from pathlib import Path

def generate_tree(directory: Path, prefix: str = "", output_file=None, ignore_self=True):
    """
    Рекурсивно генерирует дерево каталогов и записывает его в файл.
    
    :param directory: целевая директория (Path)
    :param prefix: префикс для отступов (используется при рекурсии)
    :param output_file: файловый объект для записи
    :param ignore_self: игнорировать ли файл structure.txt (чтобы не включать его в дерево)
    """
    # Получаем отсортированный список элементов (папки и файлы раздельно)
    try:
        items = list(directory.iterdir())
    except PermissionError:
        # Пропускаем недоступные папки
        output_file.write(f"{prefix}[Нет доступа]\n")
        return
    except Exception as e:
        output_file.write(f"{prefix}[Ошибка: {e}]\n")
        return

    # Фильтруем файл structure.txt, если нужно
    if ignore_self:
        items = [item for item in items if item.name != "structure.txt"]

    # Сортируем: сначала папки, потом файлы, и по алфавиту
    dirs = sorted([item for item in items if item.is_dir()], key=lambda x: x.name)
    files = sorted([item for item in items if item.is_file()], key=lambda x: x.name)
    sorted_items = dirs + files

    # Проходим по элементам
    for i, item in enumerate(sorted_items):
        is_last = (i == len(sorted_items) - 1)
        # Выбираем символы ветвления
        connector = "└── " if is_last else "├── "
        output_file.write(f"{prefix}{connector}{item.name}\n")

        # Если это директория, обрабатываем её рекурсивно
        if item.is_dir():
            new_prefix = prefix + ("    " if is_last else "│   ")
            generate_tree(item, new_prefix, output_file, ignore_self)

def main():
    # Определяем целевой каталог
    if len(sys.argv) > 1:
        target_path = Path(sys.argv[1])
    else:
        target_path = Path(".")

    # Проверяем существование каталога
    if not target_path.exists():
        print(f"Ошибка: путь '{target_path}' не существует.", file=sys.stderr)
        sys.exit(1)
    if not target_path.is_dir():
        print(f"Ошибка: '{target_path}' не является каталогом.", file=sys.stderr)
        sys.exit(1)

    # Открываем файл для записи
    output_filename = "structure.txt"
    try:
        with open(output_filename, "w", encoding="utf-8") as f:
            # Записываем корневой каталог
            f.write(f"{target_path.resolve()}\n")
            generate_tree(target_path.resolve(), output_file=f)
        print(f"Дерево структуры успешно сохранено в {output_filename}")
    except IOError as e:
        print(f"Ошибка записи в файл: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()