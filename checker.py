import sys

def validate_sequence(filename):
    """
    Проверяет валидность последовательности меток в файле.
    Каждая метка — целое число от 0 до 65535.
    Метки должны отличаться строго на 3 с учётом циклического перехода:
    следующая = (предыдущая + 3) % 65536.
    Возвращает True, если вся последовательность валидна, иначе печатает ошибку и возвращает False.
    """
    prev_label = None
    with open(filename, 'r', encoding='utf-8') as f:
        for line_num, line in enumerate(f, start=1):
            line = line.strip()
            if not line:
                continue  # пропускаем пустые строки

            # Ищем первое двоеточие
            if ':' not in line:
                print(f"Ошибка в строке {line_num}: отсутствует двоеточие")
                return False

            label_str, _ = line.split(':', 1)
            label_str = label_str.strip()

            try:
                label = int(label_str)
                print(label)
            except ValueError:
                print(f"Ошибка в строке {line_num}: метка '{label_str}' не является целым числом")
                return False

            if not (0 <= label <= 65535):
                print(f"Ошибка в строке {line_num}: метка {label} вне диапазона 0–65535")
                return False

            # Проверяем последовательность
            if prev_label is not None:
                expected = (prev_label + 3) % 65536
                if label != expected:
                    print(f"Ошибка в строке {line_num}: ожидалось {expected}, получено {label}")
                    print(f"  Предыдущая метка (строка {line_num-1}): {prev_label}")
                    return False

            prev_label = label

    # Если дошли до конца без ошибок
    if prev_label is None:
        print("Файл не содержит ни одной строки с меткой (нет строк с двоеточием).")
        return False

    print("Последовательность меток валидна.")
    return True

def main():
    if len(sys.argv) != 2:
        print("Использование: python3 checker.py <имя_файла>")
        sys.exit(1)

    filename = sys.argv[1]
    try:
        is_valid = validate_sequence(filename)
    except Exception as e:
        print(f"Ошибка при работе с файлом: {e}")
        sys.exit(1)

    sys.exit(0 if is_valid else 1)

if __name__ == "__main__":
    main()