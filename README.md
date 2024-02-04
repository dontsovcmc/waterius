# Ватериус 1.1.0
## Wi-Fi приставка для импульсных счётчиков воды, газа, тепла

<a href="https://travis-ci.org/dontsovcmc/waterius" target="_blank"><img src="https://travis-ci.org/dontsovcmc/waterius.svg?branch=master"></a>

![waterius2-phone_](https://user-images.githubusercontent.com/3930496/149906477-6aa47cdd-f714-4546-85ff-5541c60260a2.jpeg)

[Еnglish](https://github.com/dontsovcmc/waterius/blob/master/English.md)

✅ Протоколы HTTP, MQTT

✅ Поддержка <a href="https://yandex.ru/alice/smart-home" targe="_blank">Умного дома Яндекса</a>, <a href="https://www.home-assistant.io/" targe="_blank">HomeAssistant</a>,  <a href="https://www.hackster.io/dontsovcmc/domoticz-4346d5" targe="_blank">Domoticz</a>, <a href="https://spruthub.ru" targe="_blank">SprutHub</a> 

✅ Личный кабинет <a href="https://waterius.ru?utm_source=github&utm_medium=link&utm_campaign=github_16092021&utm_content=github&utm_term=github" target="_blank">waterius.ru</a>

- автоматическая <a href="https://waterius.ru?utm_source=github&utm_medium=link&utm_campaign=github_16092021&utm_content=github&utm_term=github" target="_blank">сдача показаний счётчиков</a> в 100+ «водоканалов» России и СНГ.
- отправка на электронную почту
- отправка по СМС 
- телеграм бот

✅ Работает от 3-х батареек АА несколько лет

### Подходит к счётчикам
✅ Все счётчики воды с [импульсным выходом](https://github.com/dontsovcmc/waterius/issues/65) (провод торчит из корпуса)

✅ Все счётчики газа с герконом

✅ Электронные счётчики газа: Бетар СГБМ-4 [подробнее](https://github.com/dontsovcmc/waterius/issues/233)

✅ Электронные счётчики тепла: Sanext Monu CU, Берил ITELMA СТЭ 31 [подробнее](https://github.com/dontsovcmc/waterius/issues/233)

### Где купить

[waterius.ru](https://waterius.ru?utm_source=github&utm_medium=link&utm_campaign=github_16092021&utm_content=github&utm_term=github)

### Характеристики
- подключение до двух счётчиков
- 3 АА батарейки (~2-4 года работы)
- передача по Wi-Fi
- не нужно знать, вес импульса у счётчика воды (Ватериус сам определит 1 или 10л/имп)
- не нужно знать, какого типа выход: "сухой контакт" или "НАМУР"
- настраиваемый период отправки
- ручная настройка веса импульса
- дискавери для Home Assistant
- возможно указать свои: веб сервер, MQTT брокер, сервер Blynk, NTP сервер

#### Данные с Ватериуса можно увидеть:
* на сайте <a href="https://waterius.ru?utm_source=github&utm_medium=link&utm_campaign=github_16092021&utm_content=github&utm_term=github">waterius.ru</a>
* в Приложении <a href="https://yandex.ru/alice/smart-home">Умный дом с Алисой от Яндекса</a>
* в HomeAssistant [инструкция с пояснениями](Home_Assistant_setup.md), [конфигурация](https://github.com/dontsovcmc/waterius/blob/master/homeassistant.configuration.yaml), [обсуждение](https://github.com/dontsovcmc/waterius/issues/86)
* в Domoticz [инструкция](https://www.hackster.io/dontsovcmc/domoticz-4346d5)
* в SprutHub [инструкция](https://wiki.spruthub.ru/%D0%A1%D0%BE%D0%B7%D0%B4%D0%B0%D0%BD%D0%B8%D0%B5_%D0%BA%D0%BE%D0%BD%D1%82%D1%80%D0%BE%D0%BB%D0%BB%D0%B5%D1%80%D0%B0_MQTT)
* на вашем [HTTP/HTTPS сервере](https://github.com/dontsovcmc/waterius/blob/master/Export.md#%D0%BD%D0%B0%D1%81%D1%82%D1%80%D0%BE%D0%B9%D0%BA%D0%B0-%D0%BE%D1%82%D0%BF%D1%80%D0%B0%D0%B2%D0%BA%D0%B8-%D0%BF%D0%BE-http-%D1%81%D0%B2%D0%BE%D0%B9-%D1%81%D0%B5%D1%80%D0%B2%D0%B5%D1%80)
* в MQTT клиенте [описание полей и настройка](https://github.com/dontsovcmc/waterius/blob/master/Export.md#настройка-отправки-по-mqtt)
* там, куда сами запрограммируете
  * [httpwaterius](https://github.com/grffio/httpwaterius) - web сервер с простым UI от [grffio](https://github.com/grffio)
* передать сразу в управляющую компанию через сайт [waterius.ru](https://waterius.ru?utm_source=github&utm_medium=link&utm_campaign=github_16092021&utm_content=github&utm_term=github).

## Подключение и настройка
[Текстом](http://waterius.ru/manual?utm_source=github)
[Видео](https://www.youtube.com/watch?v=dsmIdWbqJ58)

## Аналоги
[Заводские и DIY](https://github.com/dontsovcmc/waterius/issues/10)

## DIY Сделать самому
[Скачать прошивки](https://github.com/dontsovcmc/waterius/releases)

Народная инструкция в инфо Телеграм чата: [waterius_forum](https://t.me/waterius_forum)
- [Список деталей и создание платы](https://github.com/dontsovcmc/waterius/blob/master/Making.md)
- [Прошивка Attiny85 и ESP](https://github.com/dontsovcmc/waterius/blob/master/Firmware.md) 
- [Установка и настройка](https://github.com/dontsovcmc/waterius/blob/master/Setup.md) 

### Геркон для газового счётчика
Корпус для геркона под газовый счетчик BK-G4T [bk-g4t-sensor.zip](https://github.com/dontsovcmc/waterius/files/10365883/bk-g4t-sensor.zip)

## Принцип работы
Счётчик импульсов состоит из двух микросхем. Attiny85 считает импульсы в режиме сна и сохраняет их в EEPROM. Раз в сутки она будит ESP8266 и слушает i2c линию. ESP8266 спрашивает у Attiny85 данные и отправляет их на сервер. После этого ESP8266 засыпает, а Attiny85 продолжает считать-считать-считать...

### Известные ошибки
- Иногда не надёжно подключается к некоторым роутера Asus, Kineetic. Укажите в настройках Вай-фая ватериуса "only G". 
- До версии 0.11.8: Иногда не подключается к Ростелекомовским роутерам: Sercomm rv6699, Innbox e70, e80 (192.168.0.1), TP-Link AX5400. Лечится указанием статического ip в настройках. Если у вас такие, напишите в теме: [Проблемы с роутерами](https://github.com/dontsovcmc/waterius/issues/131)
- До версии 0.11.7 не подключается к Wi-Fi с пробелом в названии.

## Схема
Заводская плата:
![плата ватериуса](https://github.com/dontsovcmc/waterius/blob/master/Board/board_3_3.jpg)

В репозитории ещё есть однослойная для ЛУТа.

## Разработка 
Ветка dev для pull-request
Ветка master только для публикации прошивок

В версии 1.0.0 (ветка async) требуется помощь:
- Реализовать запись лога в файл (LittleFS), чтобы можно было скачать (/logs.html)
- OTA обновления: предложить код прошивки и пример веб-сервера (можно на базе NodeMCU)

### Модификации
[ветка attiny84](https://github.com/dontsovcmc/waterius/tree/attiny84) поддерживает плату [Waterius-Attiny84-ESP12F](https://github.com/badenbaden/Waterius-Attiny84-ESP12F) с 4мя счетчиками и 2мя датчиками протечек.

[Waterius на ESP32 с NB-IoT](https://github.com/OloloevReal/Waterius32) от OloloevReal

Датчик протечек:
- На пине reset сделал [OloloevReal](https://github.com/OloloevReal), вот [схема](https://github.com/dontsovcmc/waterius/issues/51)
- Есть в клоне [Waterius-Attiny84-ESP12F](https://github.com/badenbaden/Waterius-Attiny84-ESP12F), спасибо [badenbaden]

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
- [L2jLiga](https://github.com/L2jLiga) за обновление конфигурации Home Assistant
- nyroux за корпус геркона для газового счётчика
- Anat0liyBM за поддержку discovery HA
- [vzagorovskiy](https://github.com/vzagorovskiy) за большое обновление [0.11.0](https://github.com/dontsovcmc/waterius/releases/tag/0.11.0-beta)
- [abrant-ru](https://github.com/abrant-ru) за поддержку счётчиков с выходом "открытый коллектор" (короткими импульсами)
- Даниилу Макарову за дизайн веб интерфейса
- [videlinagbm](https://github.com/videlinagbm) за верстку веб интерфейса и js код

Форумам: 
- https://electronix.ru
- https://esp8266.ru
- https://easyelectronics.ru

## Контакты

Связь: [Facebook](https://www.facebook.com/waterius), [VK](https://vk.com/waterius1), [Instagram](https://www.instagram.com/waterius.ru/)

Задавайте вопросы в Телеграм чате: [waterius_forum](https://t.me/waterius_forum)

Найденные ошибки [в issues](https://github.com/dontsovcmc/waterius/issues)
