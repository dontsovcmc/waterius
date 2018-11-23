### Aвтономное устройство для передачи показаний воды по Wi-Fi
# Вотериус 0.5
[Description in English](https://github.com/dontsovcmc/waterius/blob/master/English.md)

Поддержать проект: <a href="https://www.buymeacoffee.com/vostnod" target="_blank"><img src="https://www.buymeacoffee.com/assets/img/custom_images/white_img.png" alt="Buy Me A Coffee"></a>

#### Данные получаем:
- в приложении [Blynk.cc](http://Blynk.cc) (под [Android](https://play.google.com/store/apps/details?id=cc.blynk), [iOS](https://itunes.apple.com/us/app/blynk-control-arduino-raspberry/id808760481?ls=1&mt=8))
- по электронной почте (ежедневно, через Blynk).
- на вашем [HTTP сервере (POST запрос с JSON)](https://github.com/dontsovcmc/waterius/blob/master/Export.md)
- там, куда сами запрограммируете

#### Статьи:
[Habrahabr.com (ru)](https://habr.com/post/418573/) | [Hackster.io (en)](https://www.hackster.io/dontsovcmc/waterius-4bfaba) | [Blynk forum (en-ru)](https://community.blynk.cc/t/autonomous-impulse-counter-for-water-meters-attiny85-esp-01)

<img src="https://github.com/dontsovcmc/waterius/blob/master/files/top.jpg" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/top.jpg" width="360"/> <img src="https://github.com/dontsovcmc/waterius/blob/master/files/step02.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/step02.png" width="180"/>

Подключение двух счётчиков.
Питание: 3 обычные или 2 литиевые батарейки \*AA. Их должно хватить на несколько лет при ежедневной отправке показаний.

## Требования
- Wi-Fi рядом с Вотериусом
- Счётчики воды с выходом типа "сухой контакт" ("намур" не поддерживается).

## Изготовление
- [Список деталей и создание платы](https://github.com/dontsovcmc/waterius/blob/master/Making.md)
- [Прошивка Attiny85 и ESP](https://github.com/dontsovcmc/waterius/blob/master/Firmware.md) 
- [Установка и настройка](https://github.com/dontsovcmc/waterius/blob/master/Setup.md) 

Если хотите сделать Вотериус, напишите мне, вдруг остались платы ;).

## Принцип работы
Счётчик импульсов состоит из двух микросхем. Attiny85 считает импульсы в режиме сна и сохраняет их в EEPROM. Раз в Х минут она будит ESP8266 и слушает i2c линию. ESP8266 спрашивает у Attiny85 данные и отправляет их на сервер. После этого все микросхемы засыпают.

## Передача показаний (вручную)
Автоматическая передача в управляющие компании не реализована. 

#### АНОНС! Если хотите, чтобы показания отправлялись сами 
Заполните анкету на сайте: <a href="https://waterius.ru">waterius.ru</a>.

## Схема
<img src="https://github.com/dontsovcmc/waterius/blob/master/Board/scheme.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/Board/scheme.png" width="600"/>

<img src="https://github.com/dontsovcmc/waterius/raw/master/Board/waterius-factory-board.jpg" data-canonical-src="https://github.com/dontsovcmc/waterius/raw/master/Board/waterius-factory-board.jpg" width="400"/>

# Помочь проекту

Если вы хотели бы помочь, проекту, то вот список дел:

0. Прикрутить дешевую китайскую камеру к ESP
(надо что-то сделать тем, у кого нет проводов у счётчиков)
1. Поддержка HTTPS 
- переписать код на HTTPS
- протестировать
2. Поддержка датчика протечек
+ 1 строчка кода ([Attiny84](https://github.com/dontsovcmc/waterius/issues/41#issuecomment-439402464))
- развести плату
3. Переход на ESP32
- разобраться, как считать импульсы на сопроцессоре
- портировать код
- начертить плату, придумать корпус (распаячная коробка?)
- расширить функционал
4. OTA обновления для ESP8266-01 1Мб
- разобраться как они работают
- предложить код прошивки и пример веб сервера
5. Радиоканал вместо Wi-Fi
- сделать вместо ESP-01 плату с радиомодулем
- сделать приемник с экраном и модулем (желательно автономный, но не критично)
Хорошо бы LoraWan. Платы в Мск вот [Yotster](https://electromicro.ru/market/nodemcu/yotster-lite/) или [на Авито](https://www.avito.ru/moskva/bytovaya_elektronika?s_trg=10&q=Lora), но можно и 433
Вопрос: чтобы период передачи был одинаковый нужен кварц для МК. Иначе приемник автономным не сделать. Но может и не надо.


# Ответственность

Прошивка Вотериуса сделана на основе открытых библиотек, работоспособность которых никто не гарантирует. Я также не могу обещать, что устройство будет работать с вашем оборудованием и вы не получите ущерба как во время изготовления, так и во время эксплуатации устройства =). Пожалуйста, сообщите о любом опыте изготовления и использования [тут](https://github.com/dontsovcmc/waterius/issues). Вы поможете развитию проекта! 

[Лицензия GNU GPLv3](https://github.com/dontsovcmc/waterius/blob/master/LICENSE)

# Благодарности
Ивану Коваленко и Иван Ганжа за консультации по электротехнике

Alex Jensen, за проект [температурного датчика](https://www.cron.dk/esp8266-on-batteries-for-years-part-1), он был взят за основу.

Форумам: 
https://electronix.ru
https://esp8266.ru
https://easyelectronics.ru


## Контакты

Чат: <a href="https://gitter.im/waterius" target="_blank"><img src="https://badges.gitter.im/gitterHQ/gitter.png" data-canonical-src="https://badges.gitter.im/gitterHQ/gitter.png"/></a>

Хэштег проекта `#waterius`: [Instagram](https://www.instagram.com/explore/tags/waterius/), [Facebook](https://www.facebook.com/search/top/?q=waterius) 

Найденные ошибки [сюда](https://github.com/dontsovcmc/waterius/issues)

Связь: [Telegram](https://t.me/Dontsovcmc), [Facebook](https://facebook.com/dontsovev), [Hackster.io](https://www.hackster.io/dontsovcmc)
