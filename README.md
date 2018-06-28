### Wi-Fi модуль для счётчиков воды
# Вотериус
Версия 0.4

Простое в изготовлении автономное устройство для передачи показаний воды по Wi-Fi.

Данные смотрим в приложении [Blynk.cc](http://Blynk.cc) (под [Android](https://play.google.com/store/apps/details?id=cc.blynk), [iOS](https://itunes.apple.com/us/app/blynk-control-arduino-raspberry/id808760481?ls=1&mt=8) )
<img src="https://github.com/dontsovcmc/waterius/blob/master/files/11541426.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/11541426.png" width="64"/> 

Также можно отсылать показания на ваш TCP сервер и на электронную почту.

<img src="https://github.com/dontsovcmc/waterius/blob/master/files/top.jpg" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/top.jpg" width="360"/> 
<img src="https://github.com/dontsovcmc/waterius/blob/master/files/step02.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/step02.png" width="360"/>

Подключение двух счётчиков.
Питание: 3*AA алкалиновые или литиевые батарейки. 
Батареек должно хватить на несколько лет при ежедневной отправке показаний.

## Совместимость

Поддерживаются счётчики воды с выходом типа "сухой контакт". Выход "намур" не поддерживается.

## Установка
[Установка и настройка](https://github.com/dontsovcmc/waterius/blob/master/Setup.md)

## Принцип работы
Счётчик импульсов состоит из двух микросхем. Attiny85 считает импульсы в режиме сна и сохраняет их в EEPROM. Раз в Х минут она будит ESP8266 и слушает i2c линию. ESP8266 спрашивает у Attiny85 данные и отправляет их на сервер Blynk.cc/TCP. После этого все микросхемы засыпают.

## Передача показаний (вручную)
Автоматическая передача не реализована из-за закрытого API Правительства Москвы. 
Самый простой способ их передавать: при помощи приложения или СМС.

Для этого зарегистрируйте ваш код плательщика, отправив на номер 7377 сообщение с текстом: «вода кп ХХХХХХХХХХ квартира N», где ХХХХХХХХХХ — ваш десятизначный код плательщика, a N — номер квартиры. Чтобы подать показания счетчиков воды, отправьте на номер 7377 сообщение с текстом: «вода добавить ХХХ YYY», где ХХХ и YYY — показания счетчиков холодной и горячей воды соответственно. Вы можете подписаться на напоминания о подаче показаний счетчиков воды, отправив СМС-сообщение с текстом: «вода напомнить» на номер 7377, или узнать показания счетчиков за прошлый месяц, отправив на этот же номер сообщение с текстом: «вода инфо последний»;

[источник:](https://www.mos.ru/otvet-dom-i-dvor/kak-podat-pokazaniya-schetchikov/)

## Изготовление
Принципиальная схема:
<img src="https://github.com/dontsovcmc/waterius/blob/master/Board/scheme.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/Board/scheme.png" width="400"/>

Потребление:
* в режиме сна: 15-23 мкА
* в режиме передачи данных: 75мА (~5 секунд)

- [создание платы](https://github.com/dontsovcmc/waterius/blob/master/Making.md)
- [прошивка Attiny85 и ESP](https://github.com/dontsovcmc/waterius/blob/master/Firmware.md) 

# Ответственность

Прошивка Ватериуса сделана на основе открытых библиотек, работоспособность которых никто не гарантирует. Я также не могу обещать, что устройство будет работать с вашем оборудованием и вы не получите ущерба как во время изготовления, так и во время эксплуатации устройства =). Пожалуйста, сообщите о любом опыте изготовления и использования [тут](https://github.com/dontsovcmc/waterius/issues). Вы поможете развитию проекта! 

[Лицензия GNU GPLv3](https://github.com/dontsovcmc/waterius/blob/master/LICENSE)

# Благодарности
Ивану Коваленко и Иван Ганжа за консультации по электротехнике

Alex Jensen, за проект [температурного датчика](https://www.cron.dk/esp8266-on-batteries-for-years-part-1). он был взят за основу.

Форумам: 
https://electronix.ru
https://esp8266.ru
http://easyelectronics.ru

 



