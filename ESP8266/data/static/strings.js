
function fill_input_color(index) {
    var q = document.getElementById('input_color');
    if (index == 0) {
        q.innerHTML = 'Красный вход';
    } else if (index == 1) {
        q.innerHTML = 'Синий вход';
    }
}

const CounterName_WATER_COLD = 0;
const CounterName_WATER_HOT = 1;
const CounterName_ELECTRO = 2;
const CounterName_GAS = 3;
const CounterName_HEAT = 4;
const CounterName_PORTABLE_WATER = 5;
const CounterName_OTHER = 6;

function fill_title(q, counter_name)
{
    switch (counter_name)
    {
    case CounterName_WATER_COLD:
        q.innerHTML = "Холодная&nbspвода";
        break;
    case CounterName_WATER_HOT:
        q.innerHTML = "Горячая&nbspвода";
        break;
    case CounterName_ELECTRO:
        q.innerHTML = "Электричество";
        break;
    case CounterName_GAS:
        q.innerHTML = "Газ";
        break;
    case CounterName_HEAT:
        q.innerHTML = "Тепло";
        break;
    case CounterName_PORTABLE_WATER:
        q.innerHTML = "Питьевая&nbspвода";
        break;
    case CounterName_OTHER:
    default:
        q.innerHTML = "Другой";
    }
}
function fill_counter_title(counter_name)
{   
    var qlist = document.querySelectorAll('[id^="counter_title"]');
    qlist.forEach(function(q) {
        fill_title(q, counter_name);
    });
}

const CounterType_NAMUR = 0;
const CounterType_DISCRETE = 1;
const CounterType_ELECTRONIC = 2;
const CounterType_HALL = 3;
const CounterType_NONE = 0xFF;

function fill_counter0_title(counter_name, counter_type)
{
    var q = document.getElementById('counter0_title');
    if (counter_type == CounterType_NONE){
        q.innerHTML = "Отключён"; 
    } else {
        fill_title(q, counter_name);
    }
}

function fill_counter1_title(counter_name, counter_type)
{
    var q = document.getElementById('counter1_title');
    if (counter_type == CounterType_NONE){
        q.innerHTML = "Отключён"; 
    } else {
        fill_title(q, counter_name);
    }
}

function fill_instruction(counter_name) {
    var q = document.getElementById('counter_instruction');
    switch(counter_name) {
        case CounterName_WATER_COLD:
            q.innerHTML = "Спускайте воду в&nbspунитазе пока устройство не&nbspперенесёт вас на&nbspследующую страницу";
            break;
        case CounterName_WATER_HOT:
            q.innerHTML = "Откройте кран горячей воды пока устройство не&nbspперенесёт вас на&nbspследующую страницу";
            break;
        case CounterName_ELECTRO:
            q.innerHTML = "Включите электроприбор. После моргания светодиода должна открыться следующая страница. Если не&nbspоткрывается, значит некорректное подключение или&nbspсчётчик не&nbspподдерживается.";
            break;
        case CounterName_GAS:
            q.innerHTML = "Приход импульса от&nbspгазового счётчика долго ожидать, нажмите Пропустить и&nbspпродолжите настройку.";
            break;
        case CounterName_HEAT:
            q.innerHTML = "Приход импульса от&nbspсчётчика тепла долго ожидать, нажмите Пропустить и&nbspпродолжите настройку.";
            break;
        case CounterName_PORTABLE_WATER:
            q.innerHTML = "Откройте кран питьевой воды пока устройство не&nbspперенесёт вас на&nbspследующую страницу";
            break;
        case CounterName_OTHER:
        default:
            q.innerHTML = "При приходе импульса от&nbspсчётчика устройство перенесёт вас на&nbspследующую страницу";
    }
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

function tr(str_id) {
    var id = Number(str_id);
    switch (id) {
        case S_ANOTHER_CHANNEL: return "Канал Wi-Fi роутера отличается от текущего соединения. Если телефон потеряет связь с Ватериусом, подключитесь заново.";
        case S_WIFI_CONNECT: return "Ошибка подключения к Wi-Fi";
        case S_SETUP_COUNTERS: return "Ватериус успешно подключился к Wi-Fi. Теперь настроим счётчики.";
        case S_NEED_SETUP: return "Ватериус ещё не настроен";
        case S_CONNECTING: return "выполняется подключение...";
        case S_SETUP: return "Настроить";
        case S_LETSGO: return "Приступить";
        case S_NO_LINK: return "Ошибка связи с МК";
        case S_WL_CONNECTION_LOST: return "Ошибка подключения. Попробуйте ещё раз.<br>Если не помогло, то пропишите статический ip. Еще можно зарезервировать MAC адрес Ватериуса в роутере. Если ничего не помогло, пришлите нам <a class='link' href='http://192.168.4.1/ssid.txt'>файл</a> параметров wi-fi сетей.";
        case S_WL_WRONG_PASSWORD: return "Ошибка подключения: Некорректный пароль";
        case S_WL_IDLE_STATUS : return "Ошибка подключения: Код 0";
        case S_WL_DISCONNECTED: return "Ошибка подключения: Отключен";
        case S_WL_NO_SHIELD: return "Ошибка подключения: Код 255";
        case S_WL_SCAN_COMPLETED: return "Ошибка подключения: Код 2";
        default: 
            return "Незвестный id строки: " + toString(id);
    }
}

function fill_tr_id(status, id) {
    var q = document.getElementById(id);
    q.innerHTML = tr(status);
}