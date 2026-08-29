/*
 * Симулятор портала Ватериуса: подмена веб-сервера ЕСП.
 *
 * Перехватывает запросы страниц и /api/*, отдаёт файлы образа из /fw/
 * с подстановкой плейсхолдеров. Страницы в /fw/ лежат такими же, как в прошивке,
 * и симулятором не правятся.
 */
'use strict';

importScripts(
    './sim/generated.js',
    './sim/lib/state.js',
    './sim/lib/core.js',
    './sim/lib/processor.js',
    './sim/lib/api.js',
    './sim/lib/router.js',
    './sim/lib/store.js'
);

var FW = '/fw/';
var CHANNEL = 'waterius-sim';

self.addEventListener('install', function () {
    self.skipWaiting();
});

self.addEventListener('activate', function (event) {
    event.waitUntil(self.clients.claim());
});

var statePromise = null;

function getState() {
    if (!statePromise) {
        statePromise = self.SimStore.load().then(function (stored) {
            return stored || self.SimState.newSession(self.SimState.defaultState());
        });
    }
    return statePromise;
}

function putState(state) {
    statePromise = Promise.resolve(state);
    return self.SimStore.save(state);
}

var channel = null;

function notify() {
    try {
        if (!channel) channel = new BroadcastChannel(CHANNEL);
        channel.postMessage({ type: 'state' });
    } catch (e) {
        // BroadcastChannel есть не везде: пульт тогда обновится по своему таймеру
    }
}

function json(data, status) {
    return new Response(JSON.stringify(data), {
        status: status || 200,
        headers: { 'Content-Type': 'application/json' },
    });
}

var TYPES = {
    html: 'text/html; charset=utf-8',
    css: 'text/css; charset=utf-8',
    js: 'application/javascript; charset=utf-8',
    txt: 'text/plain; charset=utf-8',
    png: 'image/png',
    jpg: 'image/jpeg',
    svg: 'image/svg+xml',
    ico: 'image/x-icon',
};

function contentType(file) {
    var ext = file.split('.').pop().toLowerCase();
    return TYPES[ext] || 'application/octet-stream';
}

function fetchImage(file) {
    return fetch(FW + file, { cache: 'no-store' });
}

function notFound() {
    return new Response('Not found', { status: 404, headers: { 'Content-Type': 'text/plain' } });
}

/* Параметры запроса: у GET из строки, у POST из тела формы. */
function readParams(request, url) {
    var params = [];
    url.searchParams.forEach(function (value, name) { params.push({ name: name, value: value }); });

    if (request.method !== 'POST') return Promise.resolve(params);

    return request.clone().text().then(function (body) {
        new URLSearchParams(body).forEach(function (value, name) { params.push({ name: name, value: value }); });
        return params;
    });
}

/* Файл ssid.txt пишет прошивка после сканирования сетей. */
function ssidFile(state) {
    var lines = state.wifi.networks.map(function (net) {
        return net.ssid + '\t' + net.level + '\tch' + net.wifi_channel;
    });
    return new Response(lines.join('\n') + '\n', { headers: { 'Content-Type': TYPES.txt } });
}

function handleApi(request, url, state) {
    return readParams(request, url).then(function (params) {
        var result = self.SimApi.handle(url.pathname, request.method, params, state, Date.now());
        if (!result) return notFound();

        return putState(state).then(function () {
            notify();
            if (result.redirect) return Response.redirect(new URL(result.redirect, self.location.origin).href, 302);
            if (result.text !== undefined) return new Response(result.text, { headers: { 'Content-Type': TYPES.txt } });
            return json(result.json);
        });
    });
}

function handlePortal(request, url) {
    var pathname = url.pathname;

    return getState().then(function (state) {
        if (pathname.indexOf('/api/') === 0) return handleApi(request, url, state);
        if (pathname === '/ssid.txt') return ssidFile(state);

        var target = self.SimRouter.route(pathname);
        if (!target) return notFound();

        return fetchImage(target.file).then(function (response) {
            if (!response.ok) return notFound();
            if (target.type === 'file' || target.type === 'raw') {
                return response.blob().then(function (body) {
                    return new Response(body, { headers: { 'Content-Type': contentType(target.file) } });
                });
            }
            return response.text().then(function (html) {
                return new Response(self.SimProcessor.render(html, state, target.input), {
                    headers: { 'Content-Type': TYPES.html },
                });
            });
        });
    });
}

/* ---- пульт ---- */

function patch(target, source) {
    Object.keys(source || {}).forEach(function (key) {
        if (source[key] && typeof source[key] === 'object' && !Array.isArray(source[key])) {
            target[key] = target[key] || {};
            patch(target[key], source[key]);
        } else {
            target[key] = source[key];
        }
    });
}

function refreshPortalTabs() {
    return self.clients.matchAll({ type: 'window' }).then(function (clients) {
        return Promise.all(clients.map(function (client) {
            var path = new URL(client.url).pathname;
            if (path === '/') return null; // сам пульт не трогаем
            return client.navigate(client.url).catch(function () { return null; });
        }));
    });
}

function applyCommand(command, state) {
    switch (command.type) {
        case 'impulse':
            var input = command.input === 1 ? 1 : 0;
            var count = command.count || 1;
            state.attiny['impulses' + input] += count;
            return true;
        case 'patch':
            patch(state, command.state || {});
            return true;
        case 'button':
            self.SimState.newSession(state);
            return true;
        case 'reset':
            var fresh = self.SimState.newSession(self.SimState.defaultState());
            Object.keys(fresh).forEach(function (key) { state[key] = fresh[key]; });
            return true;
        default:
            return false;
    }
}

function handlePult(request, url) {
    if (url.pathname === '/sim-api/state') {
        return getState().then(function (state) { return json(state); });
    }
    if (url.pathname === '/sim-api/cmd' && request.method === 'POST') {
        return request.json().then(function (command) {
            return getState().then(function (state) {
                if (!applyCommand(command, state)) return json({ error: 'unknown command' }, 400);
                return putState(state).then(function () {
                    notify();
                    var done = command.refresh ? refreshPortalTabs() : Promise.resolve();
                    return done.then(function () { return json(state); });
                });
            });
        });
    }
    if (url.pathname === '/sim-api/refresh') {
        return refreshPortalTabs().then(function () { return json({ ok: true }); });
    }
    return notFound();
}

self.addEventListener('fetch', function (event) {
    var url = new URL(event.request.url);
    if (url.origin !== self.location.origin) return;

    var pathname = url.pathname;
    if (pathname === '/' || pathname === '/sw.js' || pathname.indexOf('/sim/') === 0) return; // страницы пульта
    if (pathname.indexOf('/fw/') === 0) return; // прямой доступ к образу, без подстановки

    if (pathname.indexOf('/sim-api/') === 0) {
        event.respondWith(handlePult(event.request, url));
        return;
    }

    event.respondWith(handlePortal(event.request, url));
});
