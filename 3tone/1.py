import os

def collect_files_simple():
    extensions = {'.hpp', '.ppp', '.txt'}
    ignore_dirs = {'build', 'include', 'docs', 'examples'}
    
    with open('3tone_heads.txt', 'w', encoding='utf-8') as out:
        # Начальный тег
        out.write('< | source headers | >\n')
        
        for root, dirs, files in os.walk('.'):
            # Пропускаем игнорируемые папки
            dirs[:] = [d for d in dirs if d not in ignore_dirs]
            
            for file in files:
                if os.path.splitext(file)[1].lower() in extensions:
                    path = os.path.join(root, file)
                    try:
                        with open(path, 'r', encoding='utf-8') as f:
                            out.write(f'\n{"="*3}\n{path}\n{"="*3}\n')
                            out.write(f.read() + '\n')
                        print(f'✓ {path}')
                    except Exception as e:
                        print(f'✗ {path} - {e}')
        
        # Конечный тег
        out.write('\n< | end of headers | >\n')

if __name__ == "__main__":
    collect_files_simple()
    print("Готово! Файлы сохранены в 3tone_files.txt")