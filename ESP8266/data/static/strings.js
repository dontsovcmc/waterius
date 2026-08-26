/*
Весь текст веб-портала, который зависит от данных.

Названия ресурсов, инструкции мастера, единицы измерения и расшифровки
кодов ошибок собраны здесь, а в разметке лежат только пустые маркеры
(`data-unit`, идентификаторы элементов) — значения подставляет JS при
загрузке страницы.

Так сделано ради перевода: второй язык — это копия одного этого файла, ни
html, ни прошивку трогать не нужно. По той же причине здесь же лежат
разделители: десятичная запятая и запятая перед единицей в других языках
выглядят иначе.
*/

/*
Название входа для подстановки в сообщения главной страницы: одно и то же
сообщение приходит и для красного, и для синего входа, отличается только
это место. Целой фразой, а не одним прилагательным: в другом языке
предлог и порядок слов свои.
*/
const INPUT_PLACE = ["на красном входе", "на синем входе"];

function fill_input_color(index) {
    var q = document.getElementById('input_color');
    if (index == 0) {
        q.innerHTML = 'Красный вход';
    } else if (index == 1) {
        q.innerHTML = 'Синий вход';
    }
}

/*
Единицы измерения. Отдельными константами, потому что одна и та же
единица встречается у нескольких ресурсов.
*/
const U_M3 = "м³";
const U_L = "л";
const U_KWH = "кВт·ч";
const U_GCAL = "Гкал";
const U_IMPULSE = "имп.";
const U_NONE = "";          // размерность неизвестна, показывать нечего

const DECIMAL_POINT = ",";  // разделитель дробной части
const UNIT_SEPARATOR = ", "; // перед единицей в подписи поля: "Показания, м³"

const CounterName_WATER_COLD = 0;
const CounterName_WATER_HOT = 1;
const CounterName_ELECTRO = 2;
const CounterName_GAS = 3;
const CounterName_HEAT_GCAL = 4;
const CounterName_PORTABLE_WATER = 5;
const CounterName_OTHER = 6;
const CounterName_HEAT_KWH = 7;

const CounterType_NAMUR = 0;
const CounterType_DISCRETE = 1;
const CounterType_ELECTRONIC = 2;
const CounterType_HALL = 3;
const CounterType_ELECTRONIC_HIGH = 4;
const CounterType_LEAKAGE = 5;
const CounterType_NONE = 0xFF;

// Спецзначения веса импульса, ESP8266/src/core/types.h
const AUTO_IMPULSE_FACTOR = 3;
const AS_COLD_CHANNEL = 7;

/*
Ресурсы: что интерфейс показывает для каждого значения CounterName.

  title       — заголовок страницы ("Холодная вода");
  option      — пункт списка "Что считает" ("Холодную воду"): падеж другой,
                поэтому строка отдельная;
  instruction — что делать на странице определения счётчика;
  unit        — единица показаний;
  pulse, pulse_div — вес импульса: значение из формы, делённое на pulse_div,
                в единицах pulse;
  delta, delta_div — расход за время настройки: импульсы * вес / delta_div;
  per_unit    — вес импульса задан в импульсах на единицу (электричество),
                тогда расход = импульсы / вес.

Пустая единица означает "не показывать": у типа «Другой» размерность знает
только владелец счётчика, и портал больше не выдаёт её за кубометры (#358).

Числа обязаны совпадать с расчётом в ESP8266/src/core/readings.cpp: там
электричество считается как импульсы/вес, остальное — импульсы*вес/1000.
Проверка — ESP8266/scripts/test_strings.js.
*/
const RESOURCES = {};

RESOURCES[CounterName_WATER_COLD] = {
    title: "Холодная&nbspвода",
    option: "Холодную воду",
    instruction: "Спускайте воду в&nbspунитазе пока устройство не&nbspперенесёт вас на&nbspследующую страницу",
    unit: U_M3, pulse: U_L, pulse_div: 1, delta: U_L, delta_div: 1
};
RESOURCES[CounterName_WATER_HOT] = {
    title: "Горячая&nbspвода",
    option: "Горячую воду",
    instruction: "Откройте кран горячей воды пока устройство не&nbspперенесёт вас на&nbspследующую страницу",
    unit: U_M3, pulse: U_L, pulse_div: 1, delta: U_L, delta_div: 1
};
RESOURCES[CounterName_PORTABLE_WATER] = {
    title: "Питьевая&nbspвода",
    option: "Питьевую воду",
    instruction: "Откройте кран питьевой воды пока устройство не&nbspперенесёт вас на&nbspследующую страницу",
    unit: U_M3, pulse: U_L, pulse_div: 1, delta: U_L, delta_div: 1
};
RESOURCES[CounterName_ELECTRO] = {
    title: "Электричество",
    option: "Электричество",
    instruction: "Включите электроприбор. После моргания светодиода должна открыться следующая страница. Если не&nbspоткрывается, значит некорректное подключение или&nbspсчётчик не&nbspподдерживается.",
    unit: U_KWH, pulse: U_KWH, pulse_div: 1, delta: U_KWH, delta_div: 1,
    per_unit: true
};
/*
Газ и тепло: вес импульса на шкале счётчика написан в единицах показаний
(0,01 м³ на импульс), а в прошивку он уходит тысячными — отсюда pulse_div.
*/
RESOURCES[CounterName_GAS] = {
    title: "Газ",
    option: "Газ",
    instruction: "Приход импульса от&nbspгазового счётчика долго ожидать, нажмите Пропустить и&nbspпродолжите настройку.",
    unit: U_M3, pulse: U_M3, pulse_div: 1000, delta: U_M3, delta_div: 1000
};
RESOURCES[CounterName_HEAT_GCAL] = {
    title: "Тепло, Гкал",
    option: "Тепло (Гкал)",
    instruction: "Приход импульса от&nbspсчётчика тепла долго ожидать, нажмите Пропустить и&nbspпродолжите настройку.",
    unit: U_GCAL, pulse: U_GCAL, pulse_div: 1000, delta: U_GCAL, delta_div: 1000
};
RESOURCES[CounterName_HEAT_KWH] = {
    title: "Тепло, кВт·ч",
    option: "Тепло (кВт·ч)",
    instruction: "Приход импульса от&nbspсчётчика тепла долго ожидать, нажмите Пропустить и&nbspпродолжите настройку.",
    unit: U_KWH, pulse: U_KWH, pulse_div: 1000, delta: U_KWH, delta_div: 1000
};
RESOURCES[CounterName_OTHER] = {
    title: "Другой",
    option: "Другой",
    instruction: "При приходе импульса от&nbspсчётчика устройство перенесёт вас на&nbspследующую страницу",
    unit: U_NONE, pulse: U_NONE, pulse_div: 1000, delta: U_NONE, delta_div: 1000
};

/*
Типы входа для списка "Тип счётчика". Дискретный и датчик Холла в списке
не предлагаются, поэтому их здесь нет.
*/
const COUNTER_TYPES = {};
COUNTER_TYPES[CounterType_NAMUR] = "Механический";
COUNTER_TYPES[CounterType_ELECTRONIC] = "Электронный";
COUNTER_TYPES[CounterType_ELECTRONIC_HIGH] = "Электронный (+)";
COUNTER_TYPES[CounterType_LEAKAGE] = "Датчик протечки";
COUNTER_TYPES[CounterType_NONE] = "Выключен";

const S_COUNTER_DISABLED = "Отключён";

/*
Тревоги не настроить: либо вес импульса ещё не определён, либо прошивка
attiny старее 41. Обновляется она только программатором, поэтому у части
устройств в поле так и останется.
*/
const S_ALARM_NOT_READY = "Тревоги недоступны: сначала настройте счётчики. " +
    "Если счётчики настроены, нужна прошивка attiny 41 или новее.";

/*
Ресурс по номеру. Неизвестное значение — это "Другой": чужое число может
прийти из настроек старой прошивки, падать из-за него страница не должна.
*/
function resource(counter_name) {
    var r = RESOURCES[Number(counter_name)];
    return r ? r : RESOURCES[CounterName_OTHER];
}

function fill_title(q, counter_name)
{
    q.innerHTML = resource(counter_name).title;
}

function fill_counter_title(counter_name)
{
    var qlist = document.querySelectorAll('[id^="counter_title"]');
    qlist.forEach(function(q) {
        fill_title(q, counter_name);
    });
}

function fill_counter0_title(counter_name, counter_type)
{
    fill_counter_n_title('counter0_title', counter_name, counter_type);
}

function fill_counter1_title(counter_name, counter_type)
{
    fill_counter_n_title('counter1_title', counter_name, counter_type);
}

function fill_counter_n_title(id, counter_name, counter_type)
{
    var q = document.getElementById(id);
    if (counter_type == CounterType_NONE) {
        q.innerHTML = S_COUNTER_DISABLED;
    } else {
        fill_title(q, counter_name);
    }
}

function fill_instruction(counter_name) {
    document.getElementById('counter_instruction').innerHTML = resource(counter_name).instruction;
}

/*
Подписи выпадающих списков на странице выбора счётчика. Значения (числа
CounterName и CounterType) остаются в разметке, текст приходит отсюда:
иначе один и тот же список пришлось бы переводить в двух местах.
*/
function fill_resource_options() {
    var names = document.getElementById('counter_name');
    if (names) {
        names.querySelectorAll('option').forEach(function(o) {
            o.textContent = resource(o.value).option;
        });
    }
    var types = document.getElementById('counter_type');
    if (types) {
        types.querySelectorAll('option').forEach(function(o) {
            var name = COUNTER_TYPES[Number(o.value)];
            if (name) o.textContent = name;
        });
    }
}

/*
Число для показа: до трёх знаков после запятой, хвостовые нули убраны.
0.01 → "0,01", 30 → "30".
*/
function format_number(value) {
    var s = Number(value).toFixed(3);
    if (s.indexOf(".") >= 0) {
        s = s.replace(/0+$/, "").replace(/\.$/, "");
    }
    return s.replace(".", DECIMAL_POINT);
}

/*
Подпись и единица порога расхода (#202). У электричества это мощность в
ваттах: там вес импульса задан наоборот, импульсами на киловатт-час, и
считается порог по другой формуле.
*/
function alarm_flow_label(counter_name) {
    return Number(counter_name) == CounterName_ELECTRO
        ? "Порог мощности, Вт"
        : "Порог расхода, л/ч";
}

/*
Страница тревог: два канала сразу, поэтому подписи заполняются по каждому
отдельно, а не общим fill_units.

ready приходит от прошивки: тревоги требуют известного веса импульса и attiny
не старее 41. Настройку, которая ничего не делает, лучше не показывать вовсе,
чем показать неработающей.
*/
function fill_alarms(counter0_name, counter1_name, ready0, ready1) {
    fill_counter0_title(counter0_name, CounterType_NAMUR);
    fill_counter1_title(counter1_name, CounterType_NAMUR);

    var names = [counter0_name, counter1_name];
    var ready = [ready0, ready1];

    document.querySelectorAll('[data-alarm-label]').forEach(function(q) {
        q.textContent = alarm_flow_label(names[Number(q.dataset.alarmLabel)]);
    });

    for (var i = 0; i < 2; i++) {
        if (!ready[i]) {
            document.getElementById('alarm' + i).classList.add('hd');
        }
    }

    if (!ready0 && !ready1) {
        var note = document.getElementById('alarm_none');
        note.textContent = S_ALARM_NOT_READY;
        note.classList.remove('hd');
    }
}

// Единица показаний ресурса
function unit_of(counter_name) {
    return resource(counter_name).unit;
}

/*
Единица в подписи поля: ", м³". У ресурса без размерности — пустая строка,
чтобы в интерфейсе не осталась висячая запятая.
*/
function unit_suffix(counter_name) {
    var u = unit_of(counter_name);
    return u ? UNIT_SEPARATOR + u : U_NONE;
}

/*
Подпись варианта веса импульса: "10 л" для воды, "0,01 м³" для газа.
У ресурса без размерности остаётся голое число — доля показаний.
*/
function pulse_weight_text(counter_name, value) {
    var r = resource(counter_name);
    var num = format_number(value / r.pulse_div);
    return r.pulse ? num + " " + r.pulse : num;
}

/*
Расход за время настройки: " = 30 л". Пустая строка, если размерность
неизвестна, вес импульса ещё не задан или импульсов не было.
*/
function delta_text(counter_name, impulses, factor) {
    var r = resource(counter_name);
    if (!r.delta || !(factor > 0) || !(impulses > 0)) {
        return U_NONE;
    }
    var value = r.per_unit ? impulses / factor : impulses * factor / r.delta_div;
    return " = " + format_number(value) + " " + r.delta;
}

/*
Вес импульса, по которому считать расход прямо сейчас.

На странице электричества пользователь вводит число сам, и показать надо
введённое, ещё не сохранённое. У остальных ресурсов в форме встречаются
спецзначения ("Авто", "как у холодной") — их разворачивает прошивка,
поэтому там берётся значение из ответа /api/status.
*/
function effective_factor(counter_name, form_value, api_factor) {
    var v = Number(form_value);
    if (resource(counter_name).per_unit) {
        return v > 0 ? v : Number(api_factor);
    }
    if (v > 0 && v != AUTO_IMPULSE_FACTOR && v != AS_COLD_CHANNEL) {
        return v;
    }
    return Number(api_factor);
}

/*
Заполняет размерности на странице. В разметке стоят пустые маркеры:

  data-unit="total"    — единица показаний с запятой: ", м³"
  data-unit="unit"     — единица показаний без запятой: "кВт·ч"
  data-unit="impulses" — "имп."

Заодно переписываются подписи вариантов веса импульса.
*/
function fill_units(counter_name) {
    document.querySelectorAll('[data-unit]').forEach(function(q) {
        switch (q.dataset.unit) {
            case 'total':
                q.textContent = unit_suffix(counter_name);
                break;
            case 'unit':
                q.textContent = unit_of(counter_name);
                break;
            case 'impulses':
                q.textContent = U_IMPULSE;
                break;
            case 'alarm_flow':
                q.textContent = alarm_flow_label(counter_name);
                break;
        }
    });
    fill_factor_options(counter_name);
}

/*
Подписи вариантов веса импульса. Значения в форме не трогаем — прошивка
ждёт те же числа, — меняем только текст: 1/10/100 превращаются в "1 л"
или "0,01 м³". Спецзначения подписаны в разметке словами.
*/
function fill_factor_options(counter_name) {
    var q = document.getElementById('factor');
    if (!q || q.tagName != 'SELECT') {
        return;
    }
    q.querySelectorAll('option').forEach(function(o) {
        var v = Number(o.value);
        if (v == AUTO_IMPULSE_FACTOR || v == AS_COLD_CHANNEL) {
            return;
        }
        o.textContent = pulse_weight_text(counter_name, v);
    });
}

const S_ANOTHER_CHANNEL = 0;
const S_WIFI_CONNECT = 1;
const S_SETUP_COUNTERS = 2;
const S_NEED_SETUP = 3;
const S_CONNECTING = 4;
const S_SETUP = 5;
const S_LETSGO = 6;
const S_NO_LINK = 7;
const S_WL_CONNECTION_LOST = 8;
const S_WL_WRONG_PASSWORD = 9;
const S_WL_IDLE_STATUS = 10;
const S_WL_DISCONNECTED = 11;
const S_WL_NO_SHIELD = 12;
const S_WL_SCAN_COMPLETED = 13;
const S_ERROR_LENGTH_ERROR = 14;
const S_ERROR_VALUE = 15;
const S_ERROR_ATTINY_ERROR = 16;
const S_ERROR_EMPTY = 17;
const S_PLEASE_RECONNECT_WIFI = 18;
const S_ERROR_NO_COMMA = 19;
const S_ERROR_TLS = 20;
const S_ERROR_PORT_IN_HOST = 21;
const S_ESP_RESTARTED = 22;
const S_FACTOR_TOO_BIG = 23;
const S_INPUT_SILENT = 24;


/*
Текст сообщения по номеру. Второй аргумент подставляется вместо "%s" -
так сообщение про вход остаётся одной строкой на оба входа.
*/
function tr(str_id, arg) {
    var text = tr_text(Number(str_id));
    return (arg === undefined) ? text : text.replace("%s", arg);
}

function tr_text(id) {
    switch (id) {
        case S_ANOTHER_CHANNEL: return "Канал Wi-Fi роутера отличается от текущего соединения. Если телефон потеряет связь с Ватериусом, подключитесь заново.";
        case S_WIFI_CONNECT: return "Ошибка подключения к Wi-Fi";
        case S_SETUP_COUNTERS: return "Ватериус успешно подключился к Wi-Fi. Теперь настроим счётчики.";
        case S_NEED_SETUP: return "Ватериус ещё не настроен";
        case S_CONNECTING: return "выполняется подключение...";
        case S_SETUP: return "Настроить";
        case S_LETSGO: return "Приступить";
        case S_NO_LINK: return "Ошибка связи с МК";
        case S_WL_CONNECTION_LOST: return "Ошибка подключения. Что может помочь:<br>1) Проверьте, что Wi-Fi не Only N, шифрование WPA, WPA2.<br>2) Выключите DHCP на этой странице, заполнив ip роутера и Ватериуса.<br>3) Пропишите MAC адрес Ватериуса в роутере.<br>4) Пришлите нам <a class='link' href='http://192.168.4.1/ssid.txt'>файл</a> параметров wi-fi сетей.";
        case S_WL_WRONG_PASSWORD: return "Ошибка подключения: Некорректный пароль";
        case S_WL_IDLE_STATUS : return "";  // Ошибка подключения: Код 0"; Не является ошибкой
        case S_WL_DISCONNECTED: return "";  // Ошибка подключения: Отключен"; Не является ошибкой
        case S_WL_NO_SHIELD: return "Ошибка подключения: Код 255";
        case S_WL_SCAN_COMPLETED: return "Ошибка подключения: Код 2";
        case S_ERROR_LENGTH_ERROR: return "Превышена длина поля";
        case S_ERROR_VALUE: return "Неверное значение";
        case S_ERROR_ATTINY_ERROR: return "Ошибка связи с attiny";
        case S_ERROR_EMPTY: return "Значение не может быть пустым";
        case S_PLEASE_RECONNECT_WIFI: return "Wi-Fi соединение разорвано. Подключитесь ещё раз к Ватериусу.";
        case S_ERROR_NO_COMMA: return "Показания воды вводятся с литрами: 123.456. Целое число обычно означает забытую запятую";
        case S_ERROR_TLS: return "Шифрованное подключение к MQTT не поддерживается. Укажите адрес без mqtts:// и ssl://";
        case S_ERROR_PORT_IN_HOST: return "Порт указывайте в отдельном поле, а не в адресе";
        case S_ESP_RESTARTED: return "Ватериус внештатно перезагрузился. Возможно, батарейки садятся.";
        case S_FACTOR_TOO_BIG: return "Счётчик %s насчитал в разы больше второго. Похоже, вес импульса завышен в 10 раз: проверьте, сколько литров на импульс написано на счётчике.";
        case S_INPUT_SILENT: return "Счётчик %s не насчитал ни одного импульса. Проверьте подключение и тип входа.";
        default:
            return "Незвестный id строки: " + String(id);
    }
}

function fill_tr_id(status, id) {
    var q = document.getElementById(id);
    q.innerHTML = tr(status);
}
