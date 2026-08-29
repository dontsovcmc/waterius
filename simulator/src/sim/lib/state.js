/*
 * Состояние симулятора: настройки Ватериуса, модель attiny и Wi-Fi.
 *
 * Настройки строятся из таблицы, снятой с прошивки (gen_from_firmware.js),
 * поэтому поля, их длины и значения по умолчанию не расходятся с Settings.
 */
(function (root) {
    'use strict';

    function G() {
        return root.SIM_GENERATED;
    }

    var WL = {
        IDLE_STATUS: 0,
        NO_SSID_AVAIL: 1,
        SCAN_COMPLETED: 2,
        CONNECTED: 3,
        CONNECT_FAILED: 4,
        CONNECTION_LOST: 5,
        WRONG_PASSWORD: 6,
        DISCONNECTED: 7,
        NO_SHIELD: 255,
    };

    // Сколько симулятор «подключается» к роутеру, мс
    var CONNECT_MS = 3000;

    function defaultSettings() {
        var sett = {};
        G().settings.forEach(function (field) {
            sett[field.name] = field.def;
        });
        return sett;
    }

    function fieldLength(name) {
        var found = G().settings.filter(function (field) {
            return field.name === name;
        })[0];
        return found ? found.len : 0;
    }

    function defaultState() {
        return {
            sett: defaultSettings(),
            attiny: {
                version: 41, // с 41 приезжают тревоги (#202); пульт умеет понизить

                model: 1,
                voltage: 3100,
                resets: 0,
                setup_started_counter: 1,
                counter_type0: G().enums.CounterType.NAMUR,
                counter_type1: G().enums.CounterType.NAMUR,
                impulses0: 0,
                impulses1: 0,
                on_pulse0: false,
                on_pulse1: false,
                adc0: 512,
                adc1: 512,
                link: true, // связь ЕСП с attiny по i2c
            },
            /*
            Снимок данных attiny на старте сеанса: в прошивке это data, а свежие
            показания - runtime_data. Типы входов в снимке обновляются на лету (#360).
            */
            session: {
                impulses0: 0,
                impulses1: 0,
                counter_type0: G().enums.CounterType.NAMUR,
                counter_type1: G().enums.CounterType.NAMUR,
            },
            wifi: {
                mac: '5C:CF:7F:2A:1B:04',
                status: WL.DISCONNECTED,
                connecting: false,
                connect_started: 0,
                outcome: 'connected', // что случится при попытке подключения
                bssid: [0, 0, 0, 0, 0, 0], // кэш быстрого коннекта, пара к wifi_channel
                networks: [
                    { ssid: 'MyHome', level: 4, wifi_channel: 6, bssid: '5c:cf:7f:aa:bb:01' },
                    { ssid: 'Keenetic-1234', level: 2, wifi_channel: 11, bssid: '5c:cf:7f:aa:bb:02' },
                    { ssid: 'TP-Link_Guest', level: 1, wifi_channel: 1, bssid: '5c:cf:7f:aa:bb:03' },
                ],
            },
            portal: { exited: false, esp_restarted: false },
        };
    }

    // Нажали кнопку на Ватериусе: новый сеанс настройки, снимок показаний свежий
    function newSession(state) {
        state.session.impulses0 = state.attiny.impulses0;
        state.session.impulses1 = state.attiny.impulses1;
        state.session.counter_type0 = state.attiny.counter_type0;
        state.session.counter_type1 = state.attiny.counter_type1;
        state.attiny.setup_started_counter++;
        state.portal.exited = false;
        state.wifi.connecting = false;
        return state;
    }

    var OUTCOMES = {
        connected: WL.CONNECTED,
        wrong_password: WL.WRONG_PASSWORD,
        no_ssid: WL.NO_SSID_AVAIL,
        connect_failed: WL.CONNECT_FAILED,
        connection_lost: WL.CONNECTION_LOST,
        idle: WL.IDLE_STATUS,
    };

    var SimState = {
        WL: WL,
        CONNECT_MS: CONNECT_MS,
        OUTCOMES: OUTCOMES,
        defaultSettings: defaultSettings,
        defaultState: defaultState,
        newSession: newSession,
        fieldLength: fieldLength,
    };

    root.SimState = SimState;
    if (typeof module !== 'undefined') module.exports = SimState;
})(typeof self !== 'undefined' ? self : globalThis);
