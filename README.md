### Wi-Fi модуль для счётчиков воды
# Вотериус 0.4.2
([English](https://github.com/dontsovcmc/waterius/blob/master/English.md))
Простое в изготовлении автономное устройство для передачи показаний воды по Wi-Fi.
Данные смотрим в приложении [Blynk.cc](http://Blynk.cc) (под [Android](https://play.google.com/store/apps/details?id=cc.blynk), [iOS](https://itunes.apple.com/us/app/blynk-control-arduino-raspberry/id808760481?ls=1&mt=8) )
<img src="https://github.com/dontsovcmc/waterius/blob/master/files/11541426.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/11541426.png" width="32"/> 

Также можно отсылать показания на ваш TCP сервер и на электронную почту.

<img src="https://github.com/dontsovcmc/waterius/blob/master/files/top.jpg" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/top.jpg" width="360"/> <img src="https://github.com/dontsovcmc/waterius/blob/master/files/step02.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/step02.png" width="180"/>

Подключение двух счётчиков.

Питание: 3\*AA алкалиновые или 2\*AA литиевые батарейки. 
Батареек должно хватить на несколько лет при ежедневной отправке показаний.

## Требования
- Wi-Fi рядом с Вотериусом
- Счётчики воды с выходом типа "сухой контакт" ("намур" не поддерживается).

## Установка
Подробная инструкция: [Установка и настройка](https://github.com/dontsovcmc/waterius/blob/master/Setup.md)

## Принцип работы
Счётчик импульсов состоит из двух микросхем. Attiny85 считает импульсы в режиме сна и сохраняет их в EEPROM. Раз в Х минут она будит ESP8266 и слушает i2c линию. ESP8266 спрашивает у Attiny85 данные и отправляет их на сервер Blynk.cc/TCP. После этого все микросхемы засыпают.

## Передача показаний (вручную)
Автоматическая передача не реализована из-за закрытого API Правительства Москвы. 
Самый простой способ их передавать: при помощи приложения или [СМС](Send.md).

## Изготовление

- [создание платы](https://github.com/dontsovcmc/waterius/blob/master/Making.md)
- [прошивка Attiny85 и ESP](https://github.com/dontsovcmc/waterius/blob/master/Firmware.md) 

Принципиальная схема:

<img src="https://github.com/dontsovcmc/waterius/blob/master/Board/scheme.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/Board/scheme.png" width="400"/>

Если хотите сделать Вотериус, напишите мне - у меня много заводских плат.

<img src="https://github.com/dontsovcmc/waterius/raw/master/Board/waterius-factory-board.jpg" data-canonical-src="https://github.com/dontsovcmc/waterius/raw/master/Board/waterius-factory-board.jpg" width="400"/>

# Ответственность

Прошивка Ватериуса сделана на основе открытых библиотек, работоспособность которых никто не гарантирует. Я также не могу обещать, что устройство будет работать с вашем оборудованием и вы не получите ущерба как во время изготовления, так и во время эксплуатации устройства =). Пожалуйста, сообщите о любом опыте изготовления и использования [тут](https://github.com/dontsovcmc/waterius/issues). Вы поможете развитию проекта! 

[Лицензия GNU GPLv3](https://github.com/dontsovcmc/waterius/blob/master/LICENSE)

# Благодарности
Ивану Коваленко и Иван Ганжа за консультации по электротехнике

Alex Jensen, за проект [температурного датчика](https://www.cron.dk/esp8266-on-batteries-for-years-part-1). он был взят за основу.

Форумам: 
https://electronix.ru
https://esp8266.ru
https://easyelectronics.ru


# Контакты

Связаться со мной: [Telegram](https://t.me/Dontsovcmc), [Facebook](https://facebook.com/dontsovev), [Hackster.io](https://www.hackster.io/dontsovcmc)

 



