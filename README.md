### Автономное устройство для передачи показаний импульсных счётчиков воды по Wi-Fi
Настройте передачу показаний в управляющую компанию на сайте <a href="https://waterius.ru?utm_source=github&utm_medium=link&utm_campaign=github_16092021&utm_content=github&utm_term=github" target="_blank">waterius.ru</a>. 112 сайтов УК по России и СНГ.
# Ватериус 0.10.5
<a href="https://travis-ci.org/dontsovcmc/waterius" target="_blank"><img src="https://travis-ci.org/dontsovcmc/waterius.svg?branch=master"></a>

![waterius2-phone_](https://user-images.githubusercontent.com/3930496/149906477-6aa47cdd-f714-4546-85ff-5541c60260a2.jpeg)

[Еnglish](https://github.com/dontsovcmc/waterius/blob/master/English.md)

### Где купить
В России: [Yandex.market](https://market.yandex.ru/store--waterius/product/920451755?businessId=1106573&sku=101280435941) | [Aliexpress](https://s.click.aliexpress.com/e/_pujmcX9) | [Wildberries](https://www.wildberries.ru/catalog/26967283/detail.aspx) | [waterius.ru](https://waterius.ru?utm_source=github&utm_medium=link&utm_campaign=github_16092021&utm_content=github&utm_term=github)
Worldwide by Wildberries: [Armenia 🇦🇲](https://am.wildberries.ru/catalog/26967283/detail.aspx?targetUrl=XS) | [Belarus 🇧🇾](https://by.wildberries.ru/catalog/26967283/detail.aspx?targetUrl=XS) | [France 🇫🇷](https://wildberries.fr/product?card=26967283) | [Germany 🇩🇪](https://de.wildberries.eu/product?card=26967283) | [Israel 🇮🇱](https://wildberries.co.il/product?card=26967283)  | [Italy 🇮🇹](https://it.wildberries.eu/product?card=26967283) | [Kazakhstan 🇰🇿](https://kz.wildberries.ru/catalog/26967283/detail.aspx?targetUrl=XS) | [Kyrgyzstan 🇰🇬](https://kg.wildberries.ru/catalog/26967283/detail.aspx?targetUrl=XS) | [Poland 🇵🇱](https://pl.wildberries.eu/product?card=26967283) | [Slovakia 🇸🇰](https://sk.wildberries.eu/product?card=26967283) | [Spain 🇪🇸](https://wildberries.es/product?card=26967283) | [Ukraine 🇺🇦](https://wildberries.ua/product?card=26967283) | [USA 🇺🇸](https://us.wildberries.ru/product?card=26967283)

### Совместимые счётчики
Все счётчики с импульсным выходом (провод торчит из корпуса)

[Таблица со ссылками на магазины](https://github.com/dontsovcmc/waterius/issues/65)

### Характеристики
- 2 счётчика воды [Список поддерживаемых счётчиков](https://github.com/dontsovcmc/waterius/issues/65)
- 3 АА батарейки (~2-4 года работы)
- передача по Wi-Fi раз в сутки
- не нужно знать, сколько литров на импульс (Ватериус сам определит 1 или 10л/имп)
- не нужно знать, какого типа выход: "сухой контакт" или "НАМУР"
- флаг низкого заряда (экспериментально)
- настраиваемый период отправки (с 0.10.1)
- ручная настройка веса импульса (с 0.10.1)

#### Данные с Ватериуса можно увидеть:
* на сайте <a href="https://waterius.ru?utm_source=github&utm_medium=link&utm_campaign=github_16092021&utm_content=github&utm_term=github">waterius.ru</a>
* в приложении [Blynk 0.1](https://blynk.io) (под [Android](https://play.google.com/store/apps/details?id=cc.blynk), [iOS](https://itunes.apple.com/us/app/blynk-control-arduino-raspberry/id808760481?ls=1&mt=8)). [Настройка Ватериуса](https://github.com/dontsovcmc/waterius/wiki/%D0%9D%D0%B0%D1%81%D1%82%D1%80%D0%BE%D0%B9%D0%BA%D0%B0-%D0%BF%D1%80%D0%B8%D0%BB%D0%BE%D0%B6%D0%B5%D0%BD%D0%B8%D1%8F-Blynk) работает только до конца 2021г!
* на вашем [HTTP/HTTPS сервере](https://github.com/dontsovcmc/waterius/blob/master/Export.md#%D0%BD%D0%B0%D1%81%D1%82%D1%80%D0%BE%D0%B9%D0%BA%D0%B0-%D0%BE%D1%82%D0%BF%D1%80%D0%B0%D0%B2%D0%BA%D0%B8-%D0%BF%D0%BE-http-%D1%81%D0%B2%D0%BE%D0%B9-%D1%81%D0%B5%D1%80%D0%B2%D0%B5%D1%80)
* в MQTT клиенте [описание полей и настройка](https://github.com/dontsovcmc/waterius/blob/master/Export.md#настройка-отправки-по-mqtt)
  * в HomeAssistant [инструкция с пояснениями](Home_Assistant_setup.md), [конфигурация](https://github.com/dontsovcmc/waterius/blob/master/homeassistant.configuration.yaml), [обсуждение](https://github.com/dontsovcmc/waterius/issues/86)
  * в Domoticz [инструкция](https://www.hackster.io/dontsovcmc/domoticz-4346d5)
* там, куда сами запрограммируете
  * [httpwaterius](https://github.com/grffio/httpwaterius) - web сервер с простым UI от [grffio](https://github.com/grffio)
* передать сразу в управляющую компанию через сайт [waterius.ru](https://waterius.ru?utm_source=github&utm_medium=link&utm_campaign=github_16092021&utm_content=github&utm_term=github).

## Подключение и настройка
[Текстом](http://waterius.ru/manual?utm_source=github)
[Видео](https://www.youtube.com/watch?v=dsmIdWbqJ58)

## Аналоги
[Заводские и DIY](https://github.com/dontsovcmc/waterius/issues/10)

## Изготовление

[Прошивки HEX, BIN](https://github.com/dontsovcmc/waterius/releases)

Народная инструкция в Телеграм чате: [waterius_forum](https://t.me/waterius_forum)
- [Список деталей и создание платы](https://github.com/dontsovcmc/waterius/blob/master/Making.md)
- [Прошивка Attiny85 и ESP](https://github.com/dontsovcmc/waterius/blob/master/Firmware.md) 
- [Установка и настройка](https://github.com/dontsovcmc/waterius/blob/master/Setup.md) 

## Принцип работы
Счётчик импульсов состоит из двух микросхем. Attiny85 считает импульсы в режиме сна и сохраняет их в EEPROM. Раз в сутки она будит ESP8266 и слушает i2c линию. ESP8266 спрашивает у Attiny85 данные и отправляет их на сервер. После этого ESP8266 засыпает, а Attiny85 продолжает считать-считать-считать...

### Известные ошибки
- Иногда (?) не подключается к Ростелекомовским роутерам: Sercomm rv6699, Innbox e70, TP-Link AX5400. Лечится указанием статического ip в настройках или включением WPA шифрования сети. Если у вас такие, напишите в теме: [Проблемы с роутерами](https://github.com/dontsovcmc/waterius/issues/131)

## Схема
Заводская плата:

<img src="https://github.com/dontsovcmc/waterius/raw/master/Board/waterius-factory-board2_bottom.jpg" data-canonical-src="https://github.com/dontsovcmc/waterius/raw/master/Board/waterius-factory-board2_bottom.jpg" width="400"/>
<img src="https://github.com/dontsovcmc/waterius/raw/master/Board/waterius-factory-board2_top.jpg" data-canonical-src="https://github.com/dontsovcmc/waterius/raw/master/Board/waterius-factory-board2_top.jpg" width="400"/>

В репозитории ещё есть однослойная для ЛУТа.

## Разработка 

Ветка dev для pull-request
Ветка master только для публикации прошивок

- Отправка лога ESP в веб-интерфейс (JS код есть, спасибо Владимиру)
- OTA обновления: предложить код прошивки и пример веб-сервера (можно на базе NodeMCU)

Решены:
- ~~Записать видео установки/настройки Ватериуса (можно сразу в FB, VK)~~, спасибо Денису С.
- ~~Поддержка HTTPS~~, спасибо [marvel-m9y](https://github.com/marvel-m9y)
- ~~Поддержка НАМУР~~, спасибо Мише и его счетчику за вдохновение
- ~~Поддержка MQTT~~, спасибо [popsodav](https://github.com/popsodav)

Датчик протечек:
- На пине reset сделал [OloloevReal](https://github.com/OloloevReal), вот [схема](https://github.com/dontsovcmc/waterius/issues/51)
- Есть в клоне [Waterius-Attiny84-ESP12F](https://github.com/badenbaden/Waterius-Attiny84-ESP12F), спасибо [badenbaden]

### Модификации
[ветка attiny84](https://github.com/dontsovcmc/waterius/tree/attiny84) поддерживает плату [Waterius-Attiny84-ESP12F](https://github.com/badenbaden/Waterius-Attiny84-ESP12F) с 4мя счетчиками и 2мя датчиками протечек.

[Waterius на ESP32 с NB-IoT](https://github.com/OloloevReal/Waterius32) от OloloevReal

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
- [grffio](https://github.com/grffio) за локальный веб-сервер
- [Игорю Вахромееву](http://vakhromeev.com) за наикрутейший редизайн настроек
- Сергею А. (г. Мурманск) за подробную инструкцию по [настройке Domoticz и NodeRed](https://www.hackster.io/dontsovcmc/domoticz-4346d5)
- [sintech](https://github.com/sintech) за найденные и исправленные баги
- [zinger76](https://github.com/zinger76) за ссылку на заказ платы и [3D модель крепления](https://github.com/dontsovcmc/waterius/blob/master/wall-mount/wall_mount.md) к стене
- [badenbaden](https://github.com/badenbaden) за дельные комментарии по производству и новую версию!
- [kick2nick](https://github.com/kick2nick) за доработки функционала.
- [foxel](https://github.com/foxel) за доработку платы.
- Пользователям, приславшим очепятки и предложения: Дмитрию (г. Москва), Сергею (г. Кострома), Александру (г. Санкт-Петербург), Сергею (г. Мурманск), Антону (г. Красноярск) и др.
- Денису С. (г. Москва) за видео установки Ватериуса.
- [ivakorin](https://github.com/ivakorin) за инструкцию к Home Assistant
- Евгению К. из Самары, Олегу из Москвы за критику и помощь с прошивкой
- Олегу К. из Республики Беларусь за [инструкцию к MajorDoMo](https://mjdm.ru/forum/viewtopic.php?p=129000#p129000)
- [Drafteed](https://github.com/Drafteed) за виджет карты России
- [neitri](https://github.com/neitri) за доработки прошивки

Форумам: 
- https://electronix.ru
- https://esp8266.ru
- https://easyelectronics.ru

## Контакты

Связь: [Facebook](https://www.facebook.com/waterius), [VK](https://vk.com/waterius1), [Instagram](https://www.instagram.com/waterius.ru/)

Задавайте вопросы в Телеграм чате: [waterius_forum](https://t.me/waterius_forum)

Найденные ошибки [в issues](https://github.com/dontsovcmc/waterius/issues)
