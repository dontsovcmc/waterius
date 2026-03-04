<?php
/**
 * Эмулятор сервера Waterius с поддержкой OTA
 * Плагин: Code Snippets (WordPress)
 *
 * Установка:
 * 1. Установить плагин Code Snippets на WordPress
 * 2. Создать новый PHP-сниппет с этим кодом
 * 3. В настройках Waterius (captive portal) указать URL: https://us.pstd.ru/cloud-waterius-ru
 * 4. Для включения/выключения OTA менять $OTA_ENABLED = true/false
 *
 * Настройки (менять здесь):
 */
$OTA_ENABLED = true;  // true = отдавать update_ota, false = обычный ответ
$OTA_TARGET_VERSION = '2.0.24'; // OTA только для этой версии ESP (пустая строка = для всех)
$OTA_MANIFEST_URL = 'https://us.pstd.ru/us-files/waterius/manifest.json'; // debug
// $OTA_MANIFEST_URL = 'https://cloud.waterius.ru/ota/manifest.json';    // release

add_action('init', function() use ($OTA_ENABLED, $OTA_TARGET_VERSION, $OTA_MANIFEST_URL) {
    // URL: https://us.pstd.ru/cloud-waterius-ru
    if ($_SERVER['REQUEST_URI'] !== '/cloud-waterius-ru') {
        return;
    }

    // Читаем тело POST запроса
    $input = file_get_contents('php://input');
    $data = json_decode($input, true);

    // Логируем входящие данные
    if ($data) {
        error_log('Waterius data: ' . print_r($data, true));
    }

    // Формируем ответ
    $response = [];

    if ($OTA_ENABLED) {
        $version_esp = isset($data['version_esp']) ? $data['version_esp'] : '';
        if ($OTA_TARGET_VERSION === '' || $version_esp === $OTA_TARGET_VERSION) {
            $response['update_ota'] = $OTA_MANIFEST_URL;
        }
    }

    header('Content-Type: application/json');
    http_response_code(200);
    echo json_encode($response, JSON_FORCE_OBJECT);
    exit;
});
