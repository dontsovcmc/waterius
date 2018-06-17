### Wi-Fi модуль для счётчиков воды
# Вотериус
Версия 0.3

Простое в изготовлении автономное устройство для передачи показаний воды по Wi-Fi.

Данные смотрим в приложении [Blynk.cc](http://Blynk.cc) (под [Android](https://play.google.com/store/apps/details?id=cc.blynk), [iOS](https://itunes.apple.com/us/app/blynk-control-arduino-raspberry/id808760481?ls=1&mt=8) )
<img src="https://github.com/dontsovcmc/ImpCounter/blob/master/files/11541426.png" data-canonical-src="https://github.com/dontsovcmc/ImpCounter/blob/master/files/11541426.png" width="64"/> 

Также можно отсылать показания на ваш TCP сервер.

<img src="https://github.com/dontsovcmc/ImpCounter/blob/master/Board/photo-ESP-01.jpg" data-canonical-src="https://github.com/dontsovcmc/ImpCounter/blob/master/Board/photo-ESP-01.jpg" width="360"/> 
<img src="https://github.com/dontsovcmc/ImpCounter/blob/master/files/blynk_main.jpg" data-canonical-src="https://github.com/dontsovcmc/ImpCounter/blob/master/files/blynk_main.jpg" width="360"/>


Питание: 3*AAA алкалиновые или литиевые батарейки. 
Батареек должно хватить на несколько лет при ежедневной отправке показаний.

Потребление:
* в режиме сна: 20 мкА
* в режиме передачи данных: 75мА (~5 секунд)

## Установка счётчика
- [установите приложение Blynk на телефон](https://www.blynk.cc/getting-started).
- создайте проект, добавьте устройство ESP8266, добавьте виртуальные пины V0 и V1
- получите уникальный ключ на эл. почту
- подключите счётчики воды к разъемам Wi-fi счётчика
- включите питание
- нажмите кнопку на корпусе - включится Веб сервер для настройки
- найдите телефоном Wi-Fi точку доступа ImpulsCounter_0.3
- откройте [http://192.168.4.1](http://192.168.4.1)
- введите: имя и пароль от Wi-Fi, свободный ip адрес для счётчика (обычно 192.168.1.x, где x > 20), текущие показания счетчиков воды в кубометрах (разделитель дробного числа - точка), кол-во литров на 1 импульс (по умолчанию 10). ([пример](https://github.com/dontsovcmc/ImpCounter/blob/master/files/wifi_setup.jpg))
- при желании получать эл. письма с показаниями введите свой эл. адрес
- нажмите ОК
- откройте воду, чтобы вылилось больше 10л воды
- через 2 минуты счетчик выйдет на связь и передаст показания в приложение Blynk
- далее он будет слать показания раз в сутки


## Принцип работы
Счётчик импульсов состоит из двух микросхем. Attiny85 считает импульсы в режиме сна и сохраняет их в EEPROM. Раз в Х минут она будит ESP8266 и слушает i2c линию. ESP8266 спрашивает у Attiny85 данные и отправляет их на сервер Blynk.cc/TCP. После этого все микросхемы засыпают.


## Изготовление
Принципиальная схема:
<img src="https://github.com/dontsovcmc/ImpCounter/blob/master/Board/scheme-ESP-01.png" data-canonical-src="https://github.com/dontsovcmc/ImpCounter/blob/master/Board/scheme-ESP-01.png" width="400"/>

- [создание платы](https://github.com/dontsovcmc/ImpCounter/blob/master/Making.md)
- [прошивка Attiny85 и ESP](https://github.com/dontsovcmc/ImpCounter/blob/master/Firmware.md) 

# Благодарности
Ивану Коваленко и Иван Ганжа за консультации по электротехнике

Alex Jensen, за проект [температурного датчика](https://www.cron.dk/esp8266-on-batteries-for-years-part-1). он был взят за основу. Правда потом переписано все, кроме работы по i2c.

Форумам: https://esp8266.ru, http://easyelectronics.ru

 



