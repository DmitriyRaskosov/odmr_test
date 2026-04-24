#!/usr/bin/env python3
"""
Скрипт для обработки набора файлов с временными метками.
Файлы группируются по каналам (ch0_, ch1_, и т.д.).
Для каждого канала смещение накапливается между файлами.
При обнаружении значения 0.0000000 смещение увеличивается на 100.0.
"""

import sys
import re
import os
from pathlib import Path
from collections import defaultdict


def parse_timestamps(content: str) -> list[float]:
    """
    Извлекает все числа из содержимого файла.
    """
    pattern = r'\d+\.\d+'
    matches = re.findall(pattern, content)
    return [float(m) for m in matches]


def process_timestamps(timestamps: list[float], initial_offset: float = 0.0) -> tuple[list[float], int, float]:
    """
    Обрабатывает список временных меток.
    
    Args:
        timestamps: список временных меток
        initial_offset: начальное смещение (накопленное с предыдущих файлов)
    
    Returns:
        (обработанные метки, количество новых нулей в этом файле, итоговое смещение)
    """
    offset = initial_offset
    zero_count = 0
    result = []
    
    for ts in timestamps:
        if ts == 0.0:
            offset += 100.0
            zero_count += 1
            result.append(ts)  # нулевое значение не изменяем
        else:
            result.append(ts + offset)
    
    return result, zero_count, offset


def format_timestamps(timestamps: list[float], original_content: str) -> str:
    """
    Форматирует обработанные временные метки, сохраняя структуру оригинала.
    """
    pattern = r'\d+\.\d+'
    parts = re.split(pattern, original_content)
    
    result_parts = []
    for i, ts in enumerate(timestamps):
        result_parts.append(parts[i])
        result_parts.append(f"{ts:.7f}")
    result_parts.append(parts[-1])
    
    return ''.join(result_parts)


def extract_channel(filename: str) -> str:
    """
    Извлекает номер канала из имени файла.
    Пример: ch0_7.txt -> ch0
    """
    match = re.match(r'(ch\d+)_.*\.txt', filename)
    if match:
        return match.group(1)
    return None


def find_channel_files(directory: str, pattern: str = r'ch\d+_\d+\.txt') -> dict[str, list[str]]:
    """
    Находит все файлы по шаблону и группирует их по каналам.
    
    Returns:
        Словарь {канал: [список файлов, отсортированный по номеру]}
    """
    channel_files = defaultdict(list)
    
    path = Path(directory)
    for file_path in path.glob('ch*_*.txt'):
        filename = file_path.name
        channel = extract_channel(filename)
        if channel:
            channel_files[channel].append(filename)
    
    # Сортируем файлы в каждом канале по номеру файла
    for channel in channel_files:
        channel_files[channel].sort(key=lambda x: int(re.search(r'_(\d+)\.txt', x).group(1)))
    
    return channel_files


def main():
    # Определяем директорию для поиска файлов
    if len(sys.argv) >= 2:
        directory = sys.argv[1]
    else:
        directory = "."  # текущая директория по умолчанию
    
    if not Path(directory).is_dir():
        print(f"Ошибка: '{directory}' не является директорией.")
        sys.exit(1)
    
    print(f"Поиск файлов в директории: {directory}")
    
    # Находим и группируем файлы по каналам
    channel_files = find_channel_files(directory)
    
    if not channel_files:
        print("Файлы не найдены. Ожидаются файлы вида ch<номер>_<номер>.txt")
        sys.exit(0)
    
    print(f"Найдено каналов: {len(channel_files)}")
    
    # Обрабатываем каждый канал отдельно
    total_processed = 0
    total_zeros = 0
    
    for channel in sorted(channel_files.keys()):
        files = channel_files[channel]
        print(f"\n{'='*60}")
        print(f"Обработка канала {channel}: {len(files)} файлов")
        print(f"{'='*60}")
        
        channel_offset = 0.0  # Начальное смещение для канала
        channel_zeros = 0
        
        for filename in files:
            filepath = Path(directory) / filename
            
            try:
                # Читаем файл
                with open(filepath, 'r', encoding='utf-8') as f:
                    original_content = f.read()
                
                # Извлекаем временные метки
                timestamps = parse_timestamps(original_content)
                
                # Обрабатываем с текущим смещением канала
                processed, new_zeros, channel_offset = process_timestamps(timestamps, channel_offset)
                
                if new_zeros > 0:
                    print(f"  {filename}: найдено {new_zeros} нулевых значений, смещение теперь +{channel_offset}")
                    
                    # Форматируем и сохраняем результат
                    result_content = format_timestamps(processed, original_content)
                    
                    with open(filepath, 'w', encoding='utf-8') as f:
                        f.write(result_content)
                    
                    channel_zeros += new_zeros
                    total_processed += 1
                else:
                    print(f"  {filename}: нулевых значений нет, применяется смещение +{channel_offset}")
                    
                    if channel_offset > 0:
                        # Даже если нет новых нулей, применяем накопленное смещение
                        result_content = format_timestamps(processed, original_content)
                        
                        with open(filepath, 'w', encoding='utf-8') as f:
                            f.write(result_content)
                        
                        total_processed += 1
                        
            except Exception as e:
                print(f"  Ошибка при обработке файла {filename}: {e}")
                continue
        
        total_zeros += channel_zeros
        print(f"Итого для канала {channel}: {channel_zeros} нулевых значений, итоговое смещение +{channel_offset}")
    
    print(f"\n{'='*60}")
    print(f"Обработка завершена!")
    print(f"Обработано файлов: {total_processed}")
    print(f"Всего нулевых значений: {total_zeros}")
    print(f"{'='*60}")


if __name__ == "__main__":
    main()