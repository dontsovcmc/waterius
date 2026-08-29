/*
 * Хранилище состояния симулятора: IndexedDB.
 *
 * Service worker не видит заголовок Cookie - браузер его туда не отдаёт,
 * а состояние нужно именно воркеру: это он отрисовывает страницы.
 * Свойства те же: данные лежат в браузере, на сервер не уходят, чистятся сбросом.
 */
(function (root) {
    'use strict';

    var DB_NAME = 'waterius-sim';
    var STORE = 'state';
    var KEY = 'current';

    function openDb() {
        return new Promise(function (resolve, reject) {
            var request = indexedDB.open(DB_NAME, 1);
            request.onupgradeneeded = function () {
                request.result.createObjectStore(STORE);
            };
            request.onsuccess = function () { resolve(request.result); };
            request.onerror = function () { reject(request.error); };
        });
    }

    function withStore(mode, action) {
        return openDb().then(function (db) {
            return new Promise(function (resolve, reject) {
                var tx = db.transaction(STORE, mode);
                var request = action(tx.objectStore(STORE));
                tx.oncomplete = function () { db.close(); resolve(request ? request.result : undefined); };
                tx.onerror = function () { db.close(); reject(tx.error); };
            });
        });
    }

    function load() {
        return withStore('readonly', function (store) { return store.get(KEY); });
    }

    function save(state) {
        return withStore('readwrite', function (store) { return store.put(state, KEY); });
    }

    function clear() {
        return withStore('readwrite', function (store) { return store.delete(KEY); });
    }

    var SimStore = { load: load, save: save, clear: clear };
    root.SimStore = SimStore;
    if (typeof module !== 'undefined') module.exports = SimStore;
})(typeof self !== 'undefined' ? self : globalThis);
