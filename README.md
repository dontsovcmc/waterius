# Wi-Fi модуль для счётчиков воды
## (проект в стадии разработки)
Автономное устройство, передающее показания воды по Wi-fi в запущенный вами Телеграм-бот.

<img src="https://github.com/dontsovcmc/ImpCounter/blob/master/Board/photo-ESP-01.jpg" data-canonical-src="https://github.com/dontsovcmc/ImpCounter/blob/master/Board/photo-ESP-01.jpg" width="400"/> <img src="https://github.com/dontsovcmc/ImpCounter/blob/master/Board/scheme-ESP-01.png" data-canonical-src="https://github.com/dontsovcmc/ImpCounter/blob/master/Board/scheme-ESP-01.png" width="400"/>


Потребление
* в режиме сна: 20 мкА
* в режиме передачи данных: 75мА (~3 секунды)

Питание 3*AAA алкалиновые или литиевые батарейки.

## Принцип работы
Счётчик импульсов состоит из 2-х микросхем. Attiny85 считает импульсы в режиме сна. Раз в Х минут  просыпается и сохраняет текущее значение в буфер. Раз в Y минут при очередном просыпании она будит ESP8266 импульсом на Reset и слушает i2c линию. Проснувшись, ESP8266 спрашивает у Attiny85 данные и отправляет их на сервер. После этого все микросхемы засыпают.

## Установка счётчика
- получите ID и пароль для счетчика у телеграм бота по команде /newid (выполняется только для указанного при запуске администратора)
- подключите счётчики воды к разъемам Wi-fi счётчика
- включите питание
- нажмите кнопку на корпусе - включится Веб сервер для настройки
- найдите телефоном Wi-Fi точку доступа ImpulsCounter_X
- откройте http://192.168.4.1
- введите имя и пароль от Wi-Fi, статический ip Wi-fi счётчика, ID счетчика и пароль к серверу
- нажмите ОК


## Изготовление
- [создание платы](https://github.com/dontsovcmc/ImpCounter/blob/master/Making.md)
- [прошивка Attiny85 и ESP](https://github.com/dontsovcmc/ImpCounter/blob/master/Firmware.md) 
- [запуск сервера](https://github.com/dontsovcmc/ImpCounter/blob/master/Server.md) 

# Благодарности
Alex Jensen, за проект [температурного датчика](https://www.cron.dk/esp8266-on-batteries-for-years-part-1). именно он был взят за основу и добавлены прерывания


