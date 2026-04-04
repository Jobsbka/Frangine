import os

def collect_files_simple():
    extensions = {'.py', '.html', '.asm', '.js', '.jsx', '.cpp','.hpp', '.md', '.txt'}
    ignore_dirs = {'tenv', 'node_modules', 'genthree'}
    
    with open('backend_files.txt', 'w', encoding='utf-8') as out:
        for root, dirs, files in os.walk('.'):
            # Пропускаем игнорируемые папки
            dirs[:] = [d for d in dirs if d not in ignore_dirs]
            
            for file in files:
                if os.path.splitext(file)[1].lower() in extensions:
                    path = os.path.join(root, file)
                    try:
                        with open(path, 'r', encoding='utf-8') as f:
                            out.write(f'\n{"="*60}\n{path}\n{"="*60}\n')
                            out.write(f.read() + '\n')
                        print(f'✓ {path}')
                    except Exception as e:
                        print(f'✗ {path} - {e}')

if __name__ == "__main__":
    collect_files_simple()
    print("Готово! Файлы сохранены в backend_files.txt")