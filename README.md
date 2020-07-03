### Aвтономное устройство для передачи показаний воды по Wi-Fi

# Ватериус 0.10.0
<a href="https://travis-ci.org/dontsovcmc/waterius" target="_blank"><img src="https://travis-ci.org/dontsovcmc/waterius.svg?branch=master"></a> <a href="https://gitter.im/waterius" target="_blank"><img src="https://badges.gitter.im/gitterHQ/gitter.png" data-canonical-src="https://badges.gitter.im/gitterHQ/gitter.png"/></a>

[Еnglish](https://github.com/dontsovcmc/waterius/blob/master/English.md)

WiFi передача показаний воды. Приставка для импульсных счётчиков воды. Простое [подключение](https://waterius.ru/manual).

### Характеристики
- 2 счётчика воды
- передача по Wi-Fi раз в сутки
- питание 3 АА батарейки (~2-4 года работы)
- не нужно знать, сколько литров на импульс (Ватериус сам определит 1 или 10л/имп)
- не нужно знать, какого типа выход: "сухой контакт" или "НАМУР"
- детектор низкого заряда (экспериментально)

[Список поддерживаемых счётчиков](https://github.com/dontsovcmc/waterius/issues/65)

### Известные ошибки
- Иногда (?) не подключается к Ростелекомовским роутерам: Sercomm rv6699, Innbox e70. Если у вас такие, напишите, о вашем опыте в теме: [Проблемы с роутерами](https://github.com/dontsovcmc/waterius/issues/131)

#### Данные с Ватериуса можно увидеть:
* на сайте <a href="https://waterius.ru">waterius.ru</a>
* в приложении [Blynk.cc](http://Blynk.cc) (под [Android](https://play.google.com/store/apps/details?id=cc.blynk), [iOS](https://itunes.apple.com/us/app/blynk-control-arduino-raspberry/id808760481?ls=1&mt=8))
* на вашем [HTTP/HTTPS сервере (POST запрос с JSON)](https://github.com/dontsovcmc/waterius/blob/master/Export.md)
* в MQTT клиенте [настройка](https://github.com/dontsovcmc/waterius/blob/master/Mqtt.md)
  * в HomeAssistant [конфигурация](https://github.com/dontsovcmc/waterius/blob/master/homeassistant.configuration.yaml), [обсуждение](https://github.com/dontsovcmc/waterius/issues/86)
  * в Domoticz [инструкция](https://www.hackster.io/dontsovcmc/domoticz-4346d5)
* там, куда сами запрограммируете
  * [httpwaterius](https://github.com/grffio/httpwaterius) - web сервер с простым UI от [grffio](https://github.com/grffio)

## Передача показаний в упр. компании
Автоматическая передача показаний реализована через сайт [waterius.ru](http://waterius.ru).
* Приложение от mos.ru ([android](https://play.google.com/store/apps/details?id=ru.altarix.mos.pgu&hl=ru))
* АО «Мосводоканал» ([onewind.mosvodokanal.ru](https://onewind.mosvodokanal.ru))
* УК «Комфорт Лыткарино» г. Лыткарино, МО
* г. Мурманск и Мурманская область ([mrivc.ru](http://www.mrivc.ru/))
* г. Ростов-на-Дону ([южныйокруг.рф](https://южныйокруг.рф))
* г. Ростов-на-Дону ([АО «Ростовводоканал»](https://vodokanalrnd.ru))
* г. Санкт-Петербург ([kvartplata.info](https://kvartplata.info))
* г. Пермь ([novogor.perm.ru](novogor.perm.ru))
* г. Пермь ([ВЦ «Инкомус»](https://inkomus.ru/ipu_uk))
* г. Химки ([himki-comfort.ru](himki-comfort.ru))
* Гранель ЖКХ (приложение) ([ggkm.ru](ggkm.ru))

Оставить заявку на отправку в ваш город можно [тут](https://github.com/dontsovcmc/waterius/issues/64) или на сайте [waterius.ru](waterius.ru). Стучитесь в личку, бывают вопросы и требуются данные для теста.

#### Статьи: 
[Habrahabr.com (ru)](https://habr.com/post/418573/) | [Hackster.io (en)](https://www.hackster.io/dontsovcmc/waterius-4bfaba) | [Blynk forum (en-ru)](https://community.blynk.cc/t/autonomous-impulse-counter-for-water-meters-attiny85-esp-01)

### КУПИТЬ
- плату -> [Telegram](http://t-do.ru/Dontsovcmc)
- конструктор -> закончились
- готовый -> [waterius.ru](https://waterius.ru)

Поддержать морально: <a href="https://www.buymeacoffee.com/vostnod" target="_blank"><img src="https://www.buymeacoffee.com/assets/img/custom_images/white_img.png" alt="Buy Me A Coffee"></a>

Купить импульсные счетчики: https://decast.com/contacts/buy

### Модицикации
Версия 0.10.0 поддерживает [Waterius-Attiny84-ESP12F](https://github.com/badenbaden/Waterius-Attiny84-ESP12F)
[Waterius на ESP32](https://github.com/OloloevReal/Waterius32) от OloloevReal

### Отзывы
Напишите, пожалуйста, отзывы: <a href="https://vk.com/topic-183491011_40049475" target="_black">VK</a> и <a href="https://www.facebook.com/waterius/reviews/" target="_black">FB</a>. Они очень важны для новых посетителей, кто не знаком с гитхабом. Спасибо!

<img src="https://github.com/dontsovcmc/waterius/blob/master/files/top.jpg" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/top.jpg" width="360"/> <img src="https://github.com/dontsovcmc/waterius/blob/master/files/step02.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/step02.png" width="180"/>

#### [Заводские и DIY аналоги](https://github.com/dontsovcmc/waterius/issues/10)

## Изготовление
- [Список деталей и создание платы](https://github.com/dontsovcmc/waterius/blob/master/Making.md)
- [Прошивка Attiny85 и ESP](https://github.com/dontsovcmc/waterius/blob/master/Firmware.md) 
- [Установка и настройка](https://github.com/dontsovcmc/waterius/blob/master/Setup.md) 

Задавайте вопросы в Телеграм чате: [waterius_forum](https://t.me/waterius_forum)

## Принцип работы
Счётчик импульсов состоит из двух микросхем. Attiny85 считает импульсы в режиме сна и сохраняет их в EEPROM. Раз в сутки она будит ESP8266 и слушает i2c линию. ESP8266 спрашивает у Attiny85 данные и отправляет их на сервер. После этого ESP8266 засыпает, а Attiny85 продолжает считать-считать-считать...

## Схема
<img src="https://github.com/dontsovcmc/waterius/blob/master/Board/scheme.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/Board/scheme.png" width="600"/>

Заводская плата:

<img src="https://github.com/dontsovcmc/waterius/raw/master/Board/waterius-factory-board2_bottom.jpg" data-canonical-src="https://github.com/dontsovcmc/waterius/raw/master/Board/waterius-factory-board2_bottom.jpg" width="400"/>
<img src="https://github.com/dontsovcmc/waterius/raw/master/Board/waterius-factory-board2_top.jpg" data-canonical-src="https://github.com/dontsovcmc/waterius/raw/master/Board/waterius-factory-board2_top.jpg" width="400"/>

В репозитории ещё есть однослойная для ЛУТа.

# Помочь проекту

- Записать видео установки/настройки Ватериуса (можно сразу в [FB](https://www.facebook.com/waterius), [VK](https://vk.com/waterius1))
- Дополнить Народную инструкцию как спаять, скомпилировать и прошить Ватериус в Arduino IDE. 

- Отправка лога ESP в вебинтерфейс (JS код есть, спасибо Владимиру)

- Добавить архив потребления (временные метки) (доработка i2c и буфера, пишите, расскажу)

- OTA обновления: предложить код прошивки и пример веб сервера

Решены:

- ~~Поддержка HTTPS~~, спасибо [marvel-m9y](https://github.com/marvel-m9y)
- ~~Поддержка НАМУР~~, спасибо Мише и его счетчику за вдохновение
- ~~Поддержка MQTT~~, спасибо [popsodav](https://github.com/popsodav)

Датчик протечек:
- На пине reset сделал [OloloevReal](https://github.com/OloloevReal), вот [схема](https://github.com/dontsovcmc/waterius/issues/51)
- На [Waterius-Attiny84-ESP12F](https://github.com/dontsovcmc/waterius/issues/41#issuecomment-439402464) сделан (но не запрограммирован) тут [Waterius-Attiny84-ESP12F](https://github.com/badenbaden/Waterius-Attiny84-ESP12F), спасибо [badenbaden]

## Новые модификации
ESP32 с камерой ([issue](https://github.com/dontsovcmc/waterius/issues/38))
0. Помочь с прошивкой HTTP клиента ESP32.
1. Помочь рассчитать конденсаторы антенны 2.4ГГц на плате.

# Ответственность

Прошивка Ватериуса сделана на основе открытых библиотек, работоспособность которых никто не гарантирует. Я также не могу обещать, что устройство будет работать с вашем оборудованием и вы не получите ущерба как во время изготовления, так и во время эксплуатации устройства =). Пожалуйста, сообщите о любом опыте изготовления и использования [тут](https://github.com/dontsovcmc/waterius/issues). Вы поможете развитию проекта! 

[Лицензия GNU GPLv3](https://github.com/dontsovcmc/waterius/blob/master/LICENSE)

# Благодарности
- [marvel-m9y](https://github.com/marvel-m9y) за поддержку HTTPS
- [OloloevReal](https://github.com/OloloevReal) за датчик протечки, работу по ESP32
- [popsodav](https://github.com/popsodav) за MQTT
- Ивану Коваленко и Иван Ганжа за консультации по электротехнике
- Alex Jensen, за проект [температурного датчика](https://www.cron.dk/esp8266-on-batteries-for-years-part-1).
- [freenetwork](https://github.com/freenetwork) за конфигурацию для HomeAssistant
- [grffio](https://github.com/grffio) за локальный вебсервер
- [Игорю Вахромееву](http://vakhromeev.com) за наикрутейший редизайн настроек (>0.9.4)
- Сергею А. (г. Мурманск) за подробную инструкцию по [настройке Domoticz и NodeRed](https://www.hackster.io/dontsovcmc/domoticz-4346d5)
- [sintech](https://github.com/sintech) за найденные и исправленные баги
- [zinger76](https://github.com/zinger76) за ссылку на заказ платы и [3D модель крепления](https://github.com/dontsovcmc/waterius/blob/master/wall-mount/wall_mount.md) к стене
- [badenbaden](https://github.com/badenbaden) за дельные комментарии по производству и новую версию!
- Пользователям, приславшим очепятки и предложения: Дмитрию (г. Москва), Сергею (г. Кострома), Александру (г. Санкт-Петербург), Сергею (г. Мурманск), Антону (г. Красноярск)

Форумам: 
- https://electronix.ru
- https://esp8266.ru
- https://easyelectronics.ru

## Контакты

Связь: [Facebook](https://www.facebook.com/waterius), [VK](https://vk.com/waterius1), [Instagram](https://www.instagram.com/waterius.ru/)

Задавайте вопросы в Телеграм чате: [waterius_forum](https://t.me/waterius_forum)

Найденные ошибки [в issues](https://github.com/dontsovcmc/waterius/issues)
