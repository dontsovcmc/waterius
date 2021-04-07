## Пример настройки Home Assistant

Предполагается, что у вас уже установлен MQTT брокер, а также соответствующая интеграция для Home Assistant.

### Создание сенсоров
Со списком доступных параметров, а также их описанием можно ознакомиться по [ссылке](https://github.com/dontsovcmc/waterius/blob/master/Export.md)

Также параметры и их текущие значения можно посмотреть при помощи [MQTT Explorer](http://mqtt-explorer.com/)



Ознакомьтесь с примером файла конфигурации [configuration.yaml](homeassistant.configuration.yaml)

Обратите внимание, что название сенсора 
```
states.sensor.napriazhenie_pitaniia_vateriusa.last_changed
```
у вас может отличаться, всё зависит от того как вы назвали сенсор слушающий топик *voltage_low*


### Создание автоматизаций

Приведённые ниже автоматизации отправляют уведомления в телеграмм о низком напряжении питания Ватериус, а также в случае если устройство не выходило на связь более 27 часов (100 000 сек) 

#### Низкое напрядение на Ватериус

**NB:** Значение сенсора в entity_id может отличаться от указанного в примере, в случае если вы вносили изменения в название сенсоров. 

```
- alias: Ватериус низкое напряжение
  description: ''
  trigger:
  - platform: state
    entity_id: sensor.napriazhenie_pitaniia_vateriusa
    to: Низкое напряжение
  condition: []
  action:
  - service: notify.telega
    data:
      message: Внимание! На ватериус зафиксированно низкое напряжение
  mode: single
```


#### Ватериус не выходил на связь более 27 часов

**NB:** Значение сенсора в entity_id может отличаться от указанного в примере, в случае если вы вносили изменения в название сенсоров. 
```
- alias: Ватериус вне сети более 27 часов
  description: ''
  trigger:
  - platform: state
    entity_id: sensor.vaterius_last_seen
    to: '100000'
  condition: []
  action:
  - service: notify.telega
    data:
      message: Внимание! Ватериус вне сети более 27 часов.
  mode: single
```
