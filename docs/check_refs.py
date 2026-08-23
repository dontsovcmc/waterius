#!/usr/bin/env python3
"""Проверка ссылок в docs/.

Документация ссылается на код в формате `путь/файл.cpp:имя_функции`. Такие ссылки
незаметно протухают: файл переименовали, функцию убрали — текст остался. Скрипт
проверяет, что каждый упомянутый файл существует, каждое имя встречается в нём, а
ссылки между документами ведут в существующие файлы.

Запуск из корня репозитория:  python3 docs/check_refs.py
"""

import glob
import os
import re
import sys

CODE_REF = re.compile(
    r"`((?:ESP8266|Attiny85)/[A-Za-z0-9_./]+?\.(?:cpp|h|ini|sh|md))(?::([A-Za-z0-9_:~]+))?`"
)
DOC_LINK = re.compile(r"\]\(([A-Za-z0-9_.\-/]+\.md)\)")

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
errors = []
checked = 0

for md in sorted(glob.glob(os.path.join(root, "docs", "*.md"))):
    name = os.path.basename(md)
    text = open(md, encoding="utf-8").read()

    for match in CODE_REF.finditer(text):
        path, symbol = match.group(1), match.group(2)
        checked += 1
        full = os.path.join(root, path)
        if not os.path.exists(full):
            errors.append(f"{name}: нет файла {path}")
            continue
        if symbol:
            body = open(full, encoding="utf-8", errors="ignore").read()
            # Имена вида Класс::метод ищем по последней части
            if symbol.split("::")[-1] not in body:
                errors.append(f"{name}: в {path} нет {symbol}")

    for match in DOC_LINK.finditer(text):
        checked += 1
        target = os.path.normpath(os.path.join(os.path.dirname(md), match.group(1)))
        if not os.path.exists(target):
            errors.append(f"{name}: битая ссылка на {match.group(1)}")

if errors:
    print(f"Проверено ссылок: {checked}. Проблем: {len(errors)}\n")
    for line in errors:
        print(f"  {line}")
    sys.exit(1)

print(f"Проверено ссылок: {checked}. Все разрешились.")
