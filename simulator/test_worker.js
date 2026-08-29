'use strict';
/*
 * Проверка service worker без браузера: код воркера запускается в песочнице Node,
 * а вместо сети ему подставляются файлы образа из ESP8266/data.
 *
 * Так тестируется склейка «маршрут - файл - подстановка - ответ», которую
 * иначе видно только глазами в браузере.
 *
 * Запуск: node simulator/test_worker.js
 */

const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.resolve(__dirname, '..');
const DATA = path.join(ROOT, 'ESP8266/data');
const ORIGIN = 'http://localhost';

function loadWorker() {
    const scope = {
        console, URL, URLSearchParams, Request, Response, Headers, Blob,
        TextEncoder, TextDecoder, JSON, Math, Date, Promise, Array, Object, String, Number,
        setTimeout, clearTimeout,
    };
    scope.self = scope;
    scope.location = new URL(ORIGIN + '/sw.js');
    scope.skipWaiting = () => {};
    scope.clients = { claim: () => Promise.resolve(), matchAll: () => Promise.resolve([]) };
    scope.BroadcastChannel = function () { this.postMessage = () => {}; };

    const listeners = {};
    scope.addEventListener = (name, fn) => { listeners[name] = fn; };

    // Вместо сети - файлы образа
    scope.fetch = (url) => {
        const file = String(url).replace(/^\/fw\//, '');
        const full = path.join(DATA, file);
        if (!fs.existsSync(full)) return Promise.resolve(new Response('', { status: 404 }));
        return Promise.resolve(new Response(fs.readFileSync(full)));
    };

    vm.createContext(scope);
    scope.importScripts = (...files) => {
        files.forEach((file) => {
            const name = file.replace('./sim/', '');
            if (name === 'generated.js') {
                scope.SIM_GENERATED = require('./gen_from_firmware.js')();
                return;
            }
            vm.runInContext(fs.readFileSync(path.join(__dirname, 'src/sim', name), 'utf8'), scope, { filename: name });
        });
    };

    vm.runInContext(fs.readFileSync(path.join(__dirname, 'src/sw.js'), 'utf8'), scope, { filename: 'sw.js' });

    // Состояние держим в памяти: IndexedDB в Node нет
    let stored = null;
    scope.SimStore = {
        load: () => Promise.resolve(stored),
        save: (state) => { stored = state; return Promise.resolve(); },
    };

    return { scope, listeners };
}

/* Отдаёт готовый Response или null, если воркер запрос не перехватил. */
async function ask(worker, url, init) {
    const request = new Request(ORIGIN + url, init);
    let answer = null;
    worker.listeners.fetch({ request, respondWith: (promise) => { answer = promise; } });
    return answer === null ? null : answer;
}

let failed = 0;

function check(name, problems) {
    if (!problems.length) {
        console.log('ok   ' + name);
    } else {
        failed++;
        console.log('FAIL ' + name);
        problems.forEach((problem) => console.log('       ' + problem));
    }
}

async function run() {
    const worker = loadWorker();
    const problems = [];

    const response = await ask(worker, '/index.html');
    if (!response) {
        problems.push('/index.html воркер не перехватил');
    } else {
        const html = await response.text();
        if (response.status !== 200) problems.push('/index.html: код ' + response.status);
        if (/%[A-Za-z_][A-Za-z_0-9]*%/.test(html)) problems.push('/index.html: остался плейсхолдер');
        if (!html.includes('<title>')) problems.push('/index.html: это не страница портала');
    }
    check('страница отдаётся с подставленными значениями', problems);

    const inputPage = await (await ask(worker, '/input/1/setup.html')).text();
    check('виртуальный путь входа ведёт на input_setup.html', [
        inputPage.includes('name="input"') ? null : 'нет поля input',
        inputPage.includes('value="1"') ? null : 'input не подставлен',
    ].filter(Boolean));

    const css = await ask(worker, '/static/style.css');
    check('статика отдаётся из образа', [
        css.status === 200 ? null : 'style.css: код ' + css.status,
        css.headers.get('Content-Type').includes('text/css') ? null : 'style.css отдан не как css',
    ].filter(Boolean));

    const status = await (await ask(worker, '/api/status/1')).json();
    check('ручка входа отвечает', [
        'impulses' in status ? null : 'нет поля impulses: ' + JSON.stringify(status),
    ].filter(Boolean));

    const saved = await (await ask(worker, '/api/save', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'waterius_on=1&waterius_email=%D0%B0@example.com&input=255',
    })).json();
    check('сохранение через POST доходит до настроек', [
        Object.keys(saved.errors || {}).length ? 'ошибки: ' + JSON.stringify(saved.errors) : null,
    ].filter(Boolean));

    const after = await (await ask(worker, '/setup_send.html')).text();
    check('сохранённое значение видно на странице', [
        after.includes('а@example.com') ? null : 'почта не подставилась в setup_send.html',
    ].filter(Boolean));

    const missing = await ask(worker, '/no-such-page.html');
    check('неизвестный адрес отдаёт 404', [
        missing.status === 404 ? null : 'код ' + missing.status,
    ].filter(Boolean));

    const pult = await ask(worker, '/');
    check('корень отдаётся пульту, а не порталу', [
        pult === null ? null : 'воркер перехватил корень',
    ].filter(Boolean));

    const state = await (await ask(worker, '/sim-api/state')).json();
    check('пульт читает состояние', [
        state.sett && state.attiny ? null : 'состояние без sett/attiny',
    ].filter(Boolean));

    const impulses = await (await ask(worker, '/sim-api/cmd', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ type: 'impulse', input: 1, count: 5 }),
    })).json();
    check('пульт подаёт импульсы', [
        impulses.attiny.impulses1 === 5 ? null : 'импульсов ' + impulses.attiny.impulses1 + ', ждали 5',
    ].filter(Boolean));

    console.log(failed ? '\nпровалено проверок: ' + failed : '\nвсе проверки пройдены');
    process.exit(failed ? 1 : 0);
}

run().catch((err) => { console.error(err); process.exit(1); });
