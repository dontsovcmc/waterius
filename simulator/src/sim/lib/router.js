/*
 * Таблица маршрутов портала. Порт server->on(...) из active_point.cpp:429-570.
 *
 * Возвращает, какой файл из образа отдать и с каким input его отрисовать.
 */
(function (root) {
    'use strict';

    // Страницы, которые отдаются с подстановкой плейсхолдеров
    var PAGES = {
        '/about.html': 'about.html',
        '/alarms.html': 'alarms.html',
        '/finish.html': 'finish.html',
        '/index.html': 'index.html',
        '/logs.html': 'logs.html',
        '/reset.html': 'reset.html',
        '/setup_send.html': 'setup_send.html',
        '/start.html': 'start.html',
        '/wifi_connect.html': 'wifi_connect.html',
        '/wifi_list.html': 'wifi_list.html',
        '/wifi_password.html': 'wifi_password.html',
        '/wifi_settings.html': 'wifi_settings.html',
    };

    // Страницы без подстановки: капча ОС и точка входа
    var RAW_PAGES = {
        '/captive_portal.html': 'captive_portal.html',
        '/captive_portal_start.html': 'captive_portal_start.html',
        '/captive_portal_error.html': 'captive_portal_error.html',
        '/captive_portal_connected.html': 'captive_portal_connected.html',
    };

    // Виртуальные пути входов: /input/N/<что-то>.html -> файл образа
    var INPUT_PAGES = {
        'setup.html': 'input_setup.html',
        'detect.html': 'input_detect.html',
        'input_electro_detect.html': 'input_electro_detect.html',
        'settings.html': 'input_settings.html',
        'input_electro_settings.html': 'input_electro_settings.html',
    };

    var FILES = {
        '/favicon.ico': 'favicon.ico',
        '/waterius_logs.txt': 'waterius_logs.txt',
        '/ssid.txt': 'ssid.txt',
    };

    function route(pathname) {
        if (PAGES[pathname]) return { type: 'page', file: PAGES[pathname] };
        if (RAW_PAGES[pathname]) return { type: 'raw', file: RAW_PAGES[pathname] };
        if (FILES[pathname]) return { type: 'file', file: FILES[pathname] };

        var input = pathname.match(/^\/input\/([01])\/([A-Za-z_0-9]+\.html)$/);
        if (input && INPUT_PAGES[input[2]]) {
            return { type: 'page', file: INPUT_PAGES[input[2]], input: Number(input[1]) };
        }

        if (/^\/static\/[A-Za-z_0-9.\-]+$/.test(pathname)) return { type: 'file', file: pathname.slice(1) };
        if (/^\/images\/[A-Za-z_0-9.\-]+$/.test(pathname)) return { type: 'file', file: pathname.slice(1) };

        // Корень в устройстве отдаёт портал, в симуляторе - пульт (см. simulator/README.md)
        return null;
    }

    function pages() {
        var list = [];
        Object.keys(PAGES).forEach(function (path) { list.push({ path: path, file: PAGES[path] }); });
        Object.keys(RAW_PAGES).forEach(function (path) { list.push({ path: path, file: RAW_PAGES[path] }); });
        [0, 1].forEach(function (input) {
            Object.keys(INPUT_PAGES).forEach(function (name) {
                list.push({ path: '/input/' + input + '/' + name, file: INPUT_PAGES[name], input: input });
            });
        });
        return list;
    }

    var SimRouter = { route: route, pages: pages };
    root.SimRouter = SimRouter;
    if (typeof module !== 'undefined') module.exports = SimRouter;
})(typeof self !== 'undefined' ? self : globalThis);
