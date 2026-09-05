/*
 * Подстановка %плейсхолдеров% в страницы портала.
 * Порт processor_main() из ESP8266/src/portal/active_point.cpp.
 */
(function (root) {
    'use strict';

    function G() {
        return root.SIM_GENERATED;
    }

    // Маска обязательных получателей тревоги, core/types.h:AlarmConfirm
    function A() {
        return G().enums.AlarmConfirm;
    }

    var NO_INPUT = 0xFF; // значение input по умолчанию, как в processor_main

    // Arduino String(float) печатает два знака после запятой
    function f2(value) {
        return Number(value || 0).toFixed(2);
    }

    // Порт replace_value(): удваивает % ради шаблонизатора ESPAsyncWebServer
    function replaceValue(value) {
        var text = String(value == null ? '' : value);
        return text.indexOf('%') < 0 ? text : text.split('%').join('%%');
    }

    // Порт template_bool(): апостроф в начале - как в прошивке
    function templateBool(value) {
        return value > 0 ? '\'value="1" checked' : '';
    }

    // Порт template_disabled(): ноль значит "выключено и потому недоступно"
    function templateDisabled(enabled) {
        return enabled > 0 ? '' : 'disabled';
    }

    // Что страница тревог может показать на входе, core/alarm.h
    function S() {
        return G().enums.AlarmInputState;
    }

    function alarmState(state, input) {
        var attiny = state.attiny;
        var sett = state.sett;
        return root.SimCore.alarmInputState(input === 0 ? attiny.counter_type0 : attiny.counter_type1,
                                            input === 0 ? sett.factor0 : sett.factor1,
                                            attiny.version);
    }

    // Порт alarm_state_class(): имена классов разбирает style.css
    function alarmStateClass(value) {
        switch (value) {
            case S().ALARM_INPUT_NO_FACTOR: return 'no-factor';
            case S().ALARM_INPUT_NO_ATTINY: return 'no-attiny';
            case S().ALARM_INPUT_NO_INPUT: return 'no-input';
            default: return '';
        }
    }

    function ip4(value) {
        var v = value >>> 0;
        return [v & 255, (v >> 8) & 255, (v >> 16) & 255, (v >> 24) & 255].join('.');
    }

    // Порт get_counter_img()
    function counterImg(input, name, ctype) {
        var CT = G().enums.CounterType;
        var CN = G().enums.CounterName;
        if (ctype === CT.ELECTRONIC) return input === 0 ? 'meter-electro-0.png' : 'meter-electro-1.png';
        if (name === CN.WATER_HOT) return input === 0 ? 'meter-hot-0.png' : 'meter-hot-1.png';
        if (name === CN.GAS) return input === 0 ? 'meter-gas-0.png' : 'meter-gas-1.png';
        return input === 0 ? 'meter-cold-0.png' : 'meter-cold-1.png';
    }

    // Порт valid_counter_type(): неизвестный тип показывается как NAMUR
    function validCounterType(ctype) {
        return root.SimCore.isValidCounterType(ctype) ? String(ctype) : String(G().enums.CounterType.NAMUR);
    }

    // Порт ветки PARAM_WIFI_CONNECT_STATUS: код строки для strings.js
    function connectStatusCode(status) {
        var WL = root.SimState.WL;
        switch (status) {
            case WL.NO_SSID_AVAIL:
            case WL.CONNECT_FAILED:
            case WL.CONNECTION_LOST:
                return '8';
            case WL.WRONG_PASSWORD:
                return '9';
            case WL.IDLE_STATUS:
                return '10';
            case WL.DISCONNECTED:
                return '11';
            case WL.NO_SHIELD:
                return '12';
            case WL.SCAN_COMPLETED:
                return '13';
            default:
                return '';
        }
    }

    /* Значение одного плейсхолдера. Порядок веток - как в processor_main. */
    function value(name, state, input) {
        var sett = state.sett;
        var attiny = state.attiny;
        var P = G().params;
        var byInput = function (zero, one) {
            if (input === 0) return zero();
            if (input === 1) return one();
            return '';
        };

        switch (name) {
            case P.PARAM_VERSION: return String(attiny.version);
            case P.PARAM_VERSION_ESP: return G().firmware_version;

            case P.PARAM_WATERIUS_HOST: return replaceValue(sett.waterius_host);
            case P.PARAM_WATERIUS_EMAIL:
                return String(sett.waterius_email).indexOf('@waterius.ru') >= 0 ? '' : replaceValue(sett.waterius_email);
            case P.PARAM_HTTP_URL: return replaceValue(sett.http_url);

            case P.PARAM_MQTT_HOST: return replaceValue(sett.mqtt_host);
            case P.PARAM_MQTT_PORT: return String(sett.mqtt_port);
            case P.PARAM_MQTT_LOGIN: return replaceValue(sett.mqtt_login);
            case P.PARAM_MQTT_PASSWORD: return sett.mqtt_password ? P.PARAM_ASTERICS : '';
            case P.PARAM_MQTT_TOPIC: return replaceValue(sett.mqtt_topic);

            case P.PARAM_INPUT: return String(input);
            case P.PARAM_CHANNEL_START:
                return byInput(function () { return f2(sett.channel0_start); },
                               function () { return f2(sett.channel1_start); });
            case P.PARAM_SERIAL:
                return byInput(function () { return replaceValue(sett.serial0); },
                               function () { return replaceValue(sett.serial1); });
            case P.PARAM_COUNTER_NAME:
                return byInput(function () { return String(sett.counter0_name); },
                               function () { return String(sett.counter1_name); });
            case P.PARAM_COUNTER0_NAME: return String(sett.counter0_name);
            case P.PARAM_COUNTER1_NAME: return String(sett.counter1_name);
            case P.PARAM_COUNTER_IMG:
                return byInput(function () { return counterImg(0, sett.counter0_name, attiny.counter_type0); },
                               function () { return counterImg(1, sett.counter1_name, attiny.counter_type1); });
            case P.PARAM_COUNTER_TYPE:
                return byInput(function () { return validCounterType(attiny.counter_type0); },
                               function () { return validCounterType(attiny.counter_type1); });
            case P.PARAM_COUNTER0_TYPE: return validCounterType(attiny.counter_type0);
            case P.PARAM_COUNTER1_TYPE: return validCounterType(attiny.counter_type1);
            case P.PARAM_FACTOR:
                return byInput(function () { return String(sett.factor0); },
                               function () { return String(sett.factor1); });

            case P.PARAM_IP: return ip4(sett.ip);
            case P.PARAM_GATEWAY: return ip4(sett.gateway);
            case P.PARAM_MASK: return ip4(sett.mask);
            case P.PARAM_MAC_ADDRESS: return state.wifi.mac;

            case P.s_period_min: return String(sett.wakeup_per_min);
            case P.PARAM_PLACE: return String(sett.place);
            case P.PARAM_COMPANY: return String(sett.company);

            case P.PARAM_MQTT_AUTO_DISCOVERY: return templateBool(sett.mqtt_auto_discovery);
            case P.PARAM_MQTT_RETAIN: return templateBool(sett.mqtt_retain);
            case P.PARAM_MQTT_DISCOVERY_TOPIC: return replaceValue(sett.mqtt_discovery_topic);

            case P.PARAM_NTP_SERVER: return String(sett.ntp_server);

            case P.PARAM_SSID: return replaceValue(sett.wifi_ssid);
            case P.PARAM_PASSWORD: return sett.wifi_password ? P.PARAM_ASTERICS : '';
            case P.PARAM_WIFI_PHY_MODE: return String(sett.wifi_phy_mode);

            case P.PARAM_WATERIUS_ON: return templateBool(sett.waterius_on);
            case P.PARAM_HTTP_ON: return templateBool(sett.http_on);
            case P.PARAM_MQTT_ON: return templateBool(sett.mqtt_on);
            case P.PARAM_DHCP_OFF: return templateBool(sett.dhcp_off);

            /* Пороги тревог (#202): страница одна на оба входа, параметры именные */
            case P.PARAM_ALARM_FLOW0: return String(sett.alarm_flow0);
            case P.PARAM_ALARM_FLOW1: return String(sett.alarm_flow1);
            case P.PARAM_ALARM_LEAK0: return String(sett.alarm_leak0);
            case P.PARAM_ALARM_LEAK1: return String(sett.alarm_leak1);
            case P.PARAM_ALARM_STOP0: return String(sett.alarm_stop0);
            case P.PARAM_ALARM_STOP1: return String(sett.alarm_stop1);
            case P.PARAM_VACATION: return templateBool(sett.vacation);

            /* Квитанция о тревоге (#202): маска обязательных получателей */
            case P.PARAM_CONFIRM_WATERIUS: return templateBool(sett.alarm_confirm & A().CONFIRM_WATERIUS);
            case P.PARAM_CONFIRM_HTTP: return templateBool(sett.alarm_confirm & A().CONFIRM_HTTP);
            case P.PARAM_CONFIRM_MQTT: return templateBool(sett.alarm_confirm & A().CONFIRM_MQTT);
            case P.PARAM_SEND_ON_CONSUMPTION: return templateBool(sett.send_on_consumption);

            /* Выключенный получатель: галочку видно, но тронуть нельзя */
            case P.PARAM_ACK_OFF_WATERIUS: return templateDisabled(sett.waterius_on);
            case P.PARAM_ACK_OFF_HTTP: return templateDisabled(sett.http_on);
            case P.PARAM_ACK_OFF_MQTT: return templateDisabled(sett.mqtt_on);

            /* Состояние входа одним классом: решает прошивка, показ решает CSS */
            case P.PARAM_ALARM_STATE0: return alarmStateClass(alarmState(state, 0));
            case P.PARAM_ALARM_STATE1: return alarmStateClass(alarmState(state, 1));
            case P.PARAM_THRESHOLDS_OFF0:
                return templateDisabled(alarmState(state, 0) !== S().ALARM_INPUT_NO_FACTOR);
            case P.PARAM_THRESHOLDS_OFF1:
                return templateDisabled(alarmState(state, 1) !== S().ALARM_INPUT_NO_FACTOR);

            case P.PARAM_BUILD_DATE_TIME: return G().build_date_time;
            case P.PARAM_FS_SIZE: return String(G().fs.totalBytes);
            case P.PARAM_FS_FREE: return String(G().fs.totalBytes - G().fs.usedBytes);
            case P.PARAM_WIFI_CONNECT_STATUS: return connectStatusCode(state.wifi.status);

            default:
                return ''; // как в прошивке: неизвестное имя - пустая строка
        }
    }

    /* Имена, которые симулятор умеет подставлять. Проверяется тестом. */
    function known() {
        var P = G().params;
        var names = [
            'PARAM_VERSION', 'PARAM_VERSION_ESP', 'PARAM_WATERIUS_HOST', 'PARAM_WATERIUS_EMAIL',
            'PARAM_HTTP_URL', 'PARAM_MQTT_HOST', 'PARAM_MQTT_PORT', 'PARAM_MQTT_LOGIN',
            'PARAM_MQTT_PASSWORD', 'PARAM_MQTT_TOPIC', 'PARAM_INPUT', 'PARAM_CHANNEL_START',
            'PARAM_SERIAL', 'PARAM_COUNTER_NAME', 'PARAM_COUNTER0_NAME', 'PARAM_COUNTER1_NAME',
            'PARAM_COUNTER_IMG', 'PARAM_COUNTER_TYPE', 'PARAM_COUNTER0_TYPE', 'PARAM_COUNTER1_TYPE',
            'PARAM_FACTOR', 'PARAM_IP', 'PARAM_GATEWAY', 'PARAM_MASK', 'PARAM_MAC_ADDRESS',
            's_period_min', 'PARAM_PLACE', 'PARAM_COMPANY', 'PARAM_MQTT_AUTO_DISCOVERY',
            'PARAM_MQTT_RETAIN', 'PARAM_MQTT_DISCOVERY_TOPIC', 'PARAM_NTP_SERVER', 'PARAM_SSID',
            'PARAM_PASSWORD', 'PARAM_WIFI_PHY_MODE', 'PARAM_WATERIUS_ON', 'PARAM_HTTP_ON',
            'PARAM_MQTT_ON', 'PARAM_DHCP_OFF', 'PARAM_BUILD_DATE_TIME', 'PARAM_FS_SIZE',
            'PARAM_FS_FREE', 'PARAM_WIFI_CONNECT_STATUS',
            'PARAM_ALARM_FLOW0', 'PARAM_ALARM_FLOW1', 'PARAM_ALARM_LEAK0', 'PARAM_ALARM_LEAK1',
            'PARAM_ALARM_STOP0', 'PARAM_ALARM_STOP1', 'PARAM_VACATION', 'PARAM_SEND_ON_CONSUMPTION',
            'PARAM_CONFIRM_WATERIUS', 'PARAM_CONFIRM_HTTP', 'PARAM_CONFIRM_MQTT',
            'PARAM_ALARM_STATE0', 'PARAM_ALARM_STATE1',
            'PARAM_THRESHOLDS_OFF0', 'PARAM_THRESHOLDS_OFF1',
            'PARAM_ACK_OFF_WATERIUS', 'PARAM_ACK_OFF_HTTP', 'PARAM_ACK_OFF_MQTT',
        ];
        return names.map(function (key) { return P[key]; }).filter(Boolean);
    }

    /* Одна проходка по файлу, как у шаблонизатора: значения не пересканируются. */
    function render(html, state, input) {
        var self_input = input === undefined ? NO_INPUT : input;
        return html.replace(/%([A-Za-z_][A-Za-z_0-9]*)%/g, function (match, name) {
            return value(name, state, self_input);
        });
    }

    var SimProcessor = {
        NO_INPUT: NO_INPUT,
        render: render,
        value: value,
        known: known,
        templateBool: templateBool,
        ip4: ip4,
    };

    root.SimProcessor = SimProcessor;
    if (typeof module !== 'undefined') module.exports = SimProcessor;
})(typeof self !== 'undefined' ? self : globalThis);
