### Aвтономное устройство для передачи показаний воды по Wi-Fi
# Вотериус 0.7 
<a href="https://travis-ci.org/dontsovcmc/waterius" target="_blank"><img src="https://travis-ci.org/dontsovcmc/waterius.svg?branch=master"></a> <a href="https://gitter.im/waterius" target="_blank"><img src="https://badges.gitter.im/gitterHQ/gitter.png" data-canonical-src="https://badges.gitter.im/gitterHQ/gitter.png"/></a>

[Еnglish](https://github.com/dontsovcmc/waterius/blob/master/English.md)

### КУПИТЬ
плату|конструктор|готовый -> [Telegram](http://t-do.ru/Dontsovcmc)

Поддержать морально: <a href="https://www.buymeacoffee.com/vostnod" target="_blank"><img src="https://www.buymeacoffee.com/assets/img/custom_images/white_img.png" alt="Buy Me A Coffee"></a>

### Характеристики:
- передача по Wi-Fi
- 2 счётчика воды ("сухой контакт" или "НАМУР")
- питание 3 АА батарейки (~4 года работы)
- до 4-х импульсов от счётчика в секунду

[Список поддерживаемых счётчиков](https://github.com/dontsovcmc/waterius/issues/65)

## Передача показаний
Автоматическая передача будет реализована через сайт [waterius.ru](http://waterius.ru).
Напишите, [куда вам требуется их отослать тут](https://github.com/dontsovcmc/waterius/issues/64)

#### Данные получаем:
- на сайт <a href="https://waterius.ru">waterius.ru</a>
- в приложении [Blynk.cc](http://Blynk.cc) (под [Android](https://play.google.com/store/apps/details?id=cc.blynk), [iOS](https://itunes.apple.com/us/app/blynk-control-arduino-raspberry/id808760481?ls=1&mt=8))
- по электронной почте (ежедневно, через Blynk).
- на вашем [HTTP/HTTPS сервере (POST запрос с JSON)](https://github.com/dontsovcmc/waterius/blob/master/Export.md)
- там, куда сами запрограммируете

#### Статьи: 
[Habrahabr.com (ru)](https://habr.com/post/418573/) | [Hackster.io (en)](https://www.hackster.io/dontsovcmc/waterius-4bfaba) | [Blynk forum (en-ru)](https://community.blynk.cc/t/autonomous-impulse-counter-for-water-meters-attiny85-esp-01)

<img src="https://github.com/dontsovcmc/waterius/blob/master/files/top.jpg" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/top.jpg" width="360"/> <img src="https://github.com/dontsovcmc/waterius/blob/master/files/step02.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/step02.png" width="180"/>


#### [Заводские и DIY аналоги](https://github.com/dontsovcmc/waterius/issues/10)

## Изготовление
- [Список деталей и создание платы](https://github.com/dontsovcmc/waterius/blob/master/Making.md)
- [Прошивка Attiny85 и ESP](https://github.com/dontsovcmc/waterius/blob/master/Firmware.md) 
- [Установка и настройка](https://github.com/dontsovcmc/waterius/blob/master/Setup.md) 

## Принцип работы
Счётчик импульсов состоит из двух микросхем. Attiny85 считает импульсы в режиме сна и сохраняет их в EEPROM. Раз в Х минут она будит ESP8266 и слушает i2c линию. ESP8266 спрашивает у Attiny85 данные и отправляет их на сервер. После этого все микросхемы засыпают.

## Схема
<img src="https://github.com/dontsovcmc/waterius/blob/master/Board/scheme.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/Board/scheme.png" width="600"/>

2-я версия заводской платы:

<img src="https://github.com/dontsovcmc/waterius/raw/master/Board/waterius-factory-board2_bottom.jpg" data-canonical-src="https://github.com/dontsovcmc/waterius/raw/master/Board/waterius-factory-board2_bottom.jpg" width="400"/>
<img src="https://github.com/dontsovcmc/waterius/raw/master/Board/waterius-factory-board2_top.jpg" data-canonical-src="https://github.com/dontsovcmc/waterius/raw/master/Board/waterius-factory-board2_top.jpg" width="400"/>

# Помочь проекту

Если вы хотели бы помочь, проекту, то вот список дел:

0. Прикрутить дешевую китайскую камеру к ESP
(надо что-то сделать тем, у кого нет проводов у счётчиков)
1. ~~Поддержка HTTPS~~, спасибо [marvel-m9y](https://github.com/marvel-m9y)
2. Поддержка датчика протечек
- На Reset пине attiny85 сделал [OloloevReal](https://github.com/OloloevReal), вот [схема](https://github.com/dontsovcmc/waterius/issues/51)
- Можно взять МК с большим количеством пинов. Для [Attiny84](https://github.com/dontsovcmc/waterius/issues/41#issuecomment-439402464) 1 строчка кода 
3. Переход на ESP32
- разобраться, как считать импульсы на сопроцессоре
- портировать код
- начертить плату, придумать корпус (распаячная коробка?)
- расширить функционал
4. OTA обновления для ESP8266-01 1Мб
- ~~разобраться как они работают~~ OTA влезет на 1Мб, если убрать HTTPS. Можно перепаять память на 4Мб.
- предложить код прошивки и пример веб сервера
5. Радиоканал вместо Wi-Fi
- сделать вместо ESP-01 плату с радиомодулем
- сделать приемник с экраном и модулем (желательно автономный, но не критично)
Хорошо бы LoraWan. Платы в Мск вот [Yotster](https://electromicro.ru/market/nodemcu/yotster-lite/) или [на Авито](https://www.avito.ru/moskva/bytovaya_elektronika?s_trg=10&q=Lora), но можно и 433
Вопрос: чтобы период передачи был одинаковый нужен кварц для МК. Иначе приемник автономным не сделать. Но может и не надо.
6. ~~Поддержка НАМУР~~

# Ответственность

Прошивка Вотериуса сделана на основе открытых библиотек, работоспособность которых никто не гарантирует. Я также не могу обещать, что устройство будет работать с вашем оборудованием и вы не получите ущерба как во время изготовления, так и во время эксплуатации устройства =). Пожалуйста, сообщите о любом опыте изготовления и использования [тут](https://github.com/dontsovcmc/waterius/issues). Вы поможете развитию проекта! 

[Лицензия GNU GPLv3](https://github.com/dontsovcmc/waterius/blob/master/LICENSE)

# Благодарности
- [marvel-m9y](https://github.com/marvel-m9y) за поддержку HTTPS
- [OloloevReal](https://github.com/OloloevReal) за датчик протечки
- Ивану Коваленко и Иван Ганжа за консультации по электротехнике
- Alex Jensen, за проект [температурного датчика](https://www.cron.dk/esp8266-on-batteries-for-years-part-1).

Форумам: 
- https://electronix.ru
- https://esp8266.ru
- https://easyelectronics.ru

## Контакты

Чат: <a href="https://gitter.im/waterius" target="_blank"><img src="https://badges.gitter.im/gitterHQ/gitter.png" data-canonical-src="https://badges.gitter.im/gitterHQ/gitter.png"/></a>

`#waterius`: [Instagram](https://www.instagram.com/explore/tags/waterius/)

Группа [Facebook](https://www.facebook.com/waterius) 

Найденные ошибки [сюда](https://github.com/dontsovcmc/waterius/issues)

Связь: [Telegram](https://t.me/Dontsovcmc), [Facebook](https://facebook.com/dontsovev)
