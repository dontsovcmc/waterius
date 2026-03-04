#!/bin/bash
set -euo pipefail

# Скрипт сборки прошивки, генерации manifest.json и копирования в папку ota/
#
# Использование:
#   ./build_and_deploy.sh <версия>     — собрать OTA с указанной версией
#   ./build_and_deploy.sh              — собрать OTA с версией из platformio.ini
#
# Примеры:
#   ./build_and_deploy.sh 2.0.25
#   ./build_and_deploy.sh 2.0.25-ota-test

ENVIRONMENT="waterius_2"
BOARD="nodemcuv2"

# Серверные пути (debug)
OTA_BASE_URL="https://us.pstd.ru/us-files/waterius"

# Серверные пути (release) — раскомментировать для продакшн
# OTA_BASE_URL="https://cloud.waterius.ru/ota"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
OTA_DIR="${PROJECT_DIR}/ota"

cd "$PROJECT_DIR"

# Извлекаем текущую версию из platformio.ini
CURRENT_VERSION=$(grep '^firmware_version' platformio.ini | sed 's/.*= *//; s/[\"\\]//g')
if [ -z "$CURRENT_VERSION" ]; then
    echo "Ошибка: не удалось определить firmware_version из platformio.ini"
    exit 1
fi

# Если передана версия — временно подменяем в platformio.ini
OTA_VERSION="${1:-$CURRENT_VERSION}"
RESTORE_VERSION=false

if [ "$OTA_VERSION" != "$CURRENT_VERSION" ]; then
    echo "=== Подмена версии: ${CURRENT_VERSION} -> ${OTA_VERSION} ==="
    cp platformio.ini platformio.ini.bak
    sed -i '' "s/^firmware_version = .*/firmware_version = \"\\\\\"${OTA_VERSION}\\\\\"\"/" platformio.ini
    RESTORE_VERSION=true
fi

restore_version() {
    if [ "$RESTORE_VERSION" = true ] && [ -f platformio.ini.bak ]; then
        cp platformio.ini.bak platformio.ini
        rm platformio.ini.bak
        echo "=== Версия восстановлена: ${CURRENT_VERSION} ==="
    fi
}
trap restore_version EXIT

# Версия для имён файлов (только цифры и точки)
VERSION_CLEAN=$(echo "$OTA_VERSION" | sed 's/[^0-9.]//g')
VERSION="$VERSION_CLEAN"

FW_FILE="${BOARD}-${VERSION}.bin"
FS_FILE="${BOARD}-${VERSION}-fs.bin"

echo "=== Версия: ${VERSION} ==="
echo "=== Environment: ${ENVIRONMENT} ==="

# Сборка прошивки
echo ""
echo "--- Сборка прошивки ---"
platformio run --environment "$ENVIRONMENT"

# Сборка файловой системы
echo ""
echo "--- Сборка файловой системы ---"
platformio run --target buildfs --environment "$ENVIRONMENT"

# Проверяем что файлы созданы (post_compile.py копирует в корень проекта)
if [ ! -f "$FW_FILE" ]; then
    echo "Ошибка: файл ${FW_FILE} не найден"
    exit 1
fi
if [ ! -f "$FS_FILE" ]; then
    echo "Ошибка: файл ${FS_FILE} не найден"
    exit 1
fi

# MD5
FW_MD5=$(md5 -q "$FW_FILE")
FS_MD5=$(md5 -q "$FS_FILE")

# Размеры
FW_SIZE=$(stat -f%z "$FW_FILE")
FS_SIZE=$(stat -f%z "$FS_FILE")

echo ""
echo "--- Файлы ---"
echo "Firmware: ${FW_FILE} (${FW_SIZE} bytes, md5: ${FW_MD5})"
echo "Filesystem: ${FS_FILE} (${FS_SIZE} bytes, md5: ${FS_MD5})"

# Создаём папку ota/ и копируем туда
mkdir -p "$OTA_DIR"
cp "$FW_FILE" "$OTA_DIR/"
cp "$FS_FILE" "$OTA_DIR/"

# Генерация manifest.json в ota/
cat > "${OTA_DIR}/manifest.json" << EOF
{
  "version": "${OTA_VERSION}",
  "firmware": {
    "url": "${OTA_BASE_URL}/${FW_FILE}",
    "size": ${FW_SIZE},
    "md5": "${FW_MD5}"
  },
  "filesystem": {
    "url": "${OTA_BASE_URL}/${FS_FILE}",
    "size": ${FS_SIZE},
    "md5": "${FS_MD5}"
  }
}
EOF

echo ""
echo "--- Результат в ota/ ---"
ls -lh "$OTA_DIR/"
echo ""
cat "${OTA_DIR}/manifest.json"

echo ""
echo "=== Готово ==="
