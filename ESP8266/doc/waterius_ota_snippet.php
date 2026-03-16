<?php
/**
 * Эмулятор сервера Waterius с поддержкой OTA v2
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
$OTA_ENABLED = true;  // true = отдавать ota, false = обычный ответ
$OTA_TARGET_VERSION = '2.0.24'; // OTA только для этой версии ESP (пустая строка = для всех)
$OTA_CONFIG = [
    'firmware' => [
        'url'  => 'https://us.pstd.ru/us-files/waterius/nodemcuv2-2.0.25.bin',
        'size' => 633344,
        'md5'  => 'f14e1220b1565f63a4a69b84cab4582d',
    ],
    'filesystem' => [
        'url'  => 'https://us.pstd.ru/us-files/waterius/nodemcuv2-2.0.25-fs.bin',
        'size' => 1024000,
        'md5'  => '6c023b5558cc375843449565ff0f2570',
    ],
];

add_action('init', function() use ($OTA_ENABLED, $OTA_TARGET_VERSION, $OTA_CONFIG) {
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
            $response['ota'] = $OTA_CONFIG;
        }
    }

    header('Content-Type: application/json');
    http_response_code(200);
    echo json_encode($response, JSON_FORCE_OBJECT);
    exit;
});
