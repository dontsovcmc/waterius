# Wi-Fi модуль для счётчиков воды
## (проект в стадии разработки)
Автономное устройство, передающее показания воды по Wi-fi:
- на сервер [Blynk.cc|http://Blynk.cc] (есть бесплатное приложение под [Android|https://play.google.com/store/apps/details?id=cc.blynk], [iOS|https://itunes.apple.com/us/app/blynk-control-arduino-raspberry/id808760481?ls=1&mt=8])
<img src="https://github.com/dontsovcmc/ImpCounter/blob/master/files/11541426.png" data-canonical-src="https://github.com/dontsovcmc/ImpCounter/blob/master/files/11541426.png" width="250" href="http://Blynk.cc"/> 

- ваш TCP сервер с запущенным Телеграм ботом (см. версию ниже 0.3) 

<img src="https://github.com/dontsovcmc/ImpCounter/blob/master/Board/photo-ESP-01.jpg" data-canonical-src="https://github.com/dontsovcmc/ImpCounter/blob/master/Board/photo-ESP-01.jpg" width="400"/> <img src="https://github.com/dontsovcmc/ImpCounter/blob/master/Board/scheme-ESP-01.png" data-canonical-src="https://github.com/dontsovcmc/ImpCounter/blob/master/Board/scheme-ESP-01.png" width="400"/>


Потребление
* в режиме сна: 20 мкА
* в режиме передачи данных: 75мА (~5 секунд)

Питание 3*AAA алкалиновые или литиевые батарейки.

## Принцип работы
Счётчик импульсов состоит из 2-х микросхем. Attiny85 считает импульсы в режиме сна. Раз в Х минут  просыпается и сохраняет текущее значение в буфер. Раз в Y минут при очередном просыпании она будит ESP8266 и слушает i2c линию. Проснувшись, ESP8266 спрашивает у Attiny85 данные и отправляет их на сервер. После этого все микросхемы засыпают.

## Установка счётчика
- [установите приложение Blynk на телефон|https://www.blynk.cc/getting-started]
- получите уникальный ключ на эл. почту
- подключите счётчики воды к разъемам Wi-fi счётчика
- включите питание
- нажмите кнопку на корпусе - включится Веб сервер для настройки
- найдите телефоном Wi-Fi точку доступа ImpulsCounter_0.3
- откройте http://192.168.4.1
- введите: имя и пароль от Wi-Fi, свободный ip адрес для счётчика (обычно 192.168.1.x, где x > 20), текущие показания счетчиков воды в кубометрах (разделитель дробного числа - точка), кол-во литров на 1 импульс (по умолчанию 10).
- нажмите ОК
- откройте воду, чтобы вылилось больше 10л воды
- через 2 минуты счетчик выйдет на связь и передаст показания в приложение Blynk
- далее он будет слать показания раз в сутки


## Изготовление
- [создание платы](https://github.com/dontsovcmc/ImpCounter/blob/master/Making.md)
- [прошивка Attiny85 и ESP](https://github.com/dontsovcmc/ImpCounter/blob/master/Firmware.md) 

# Благодарности
Alex Jensen, за проект [температурного датчика](https://www.cron.dk/esp8266-on-batteries-for-years-part-1). именно он был взят за основу и добавлены прерывания




