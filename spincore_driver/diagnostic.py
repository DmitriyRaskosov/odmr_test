#!/usr/bin/env python3
import sys
import os

# Убедитесь, что библиотека видна
os.environ['LD_LIBRARY_PATH'] = '/home/alex/odmr/spincore_driver:' + os.environ.get('LD_LIBRARY_PATH', '')

try:
    import spinapi as pb
except ImportError as e:
    print(f"Cannot import spinapi: {e}")
    sys.exit(1)

print(f"Library loaded: {pb.spinapi}")
print(f"Version: {pb.pb_get_version()}")

# Проверка без pb_stop()
print("\n=== Testing pb_init() without pb_stop() ===")
ret = pb.pb_init()
print(f"pb_init() returned: {ret}")

if ret < 0:
    print(f"ERROR CODE: {ret}")
    print(f"Error message: {pb.pb_get_error()}")
    print(f"Status message: {pb.pb_status_message()}")
    
    # Сброс состояния платы (может помочь)
    print("\n=== Attempting pb_select_board(0) ===")
    pb.pb_select_board(0)
    print("pb_select_board result:", pb.pb_get_error())
    
    print("\n=== Attempting second pb_init() ===")
    ret2 = pb.pb_init()
    print(f"Second pb_init() returned: {ret2}")

pb.pb_close()  # или pb.pb_shutdown()
