// Скрипт для отображения данных от Ватериуса в Wirenboard
// Версия: 0.1
// Дата: 2025-01-26

// Настройка
// 
// topic: Укажите значение параметра "mqtt topic" из настроек Ватериуса. Сюда Ватериус будет слать данные по mqtt.
// Описание всех полей данных и настройке: https://github.com/dontsovcmc/waterius/blob/master/Export.md
var topic = "waterius/toilet"; 


// Скрипт
var device_id = topic.replace("/", "_"); 

defineVirtualDevice(device_id, {
  title: "Ватериус (Туалет)",
  cells: {
    "Горячая вода": {
      type: "value",
      units: "m^3",
      precision: 0.001,
      order: 0,
      value: 0,
    },    

    "Холодная вода": {
      type: "value",
      units: "m^3",
      precision: 0.001,
      order: 1,
      value: 0,
    },
    
    "Вес горячей воды": {
      type: "value",
      units: "имп/литр",
      precision: 0,
      order: 2,
      value: 0,
    },

    "Вес холодной воды": {
      type: "value",
      units: "имп/литр",
      precision: 0,
      order: 2,
      value: 0,
    },

    "Период пробуждения": {
      type: "value",
      units: "мин",
      precision: 0,
      order: 2,
      value: 0,
    },
  }
});

trackMqtt(topic + "/ch0", function(message){
  dev[device_id + "/Горячая вода"] = parseFloat(message.value);
});
trackMqtt(topic + "/ch1", function(message){
  dev[device_id + "/Холодная вода"] = parseFloat(message.value);
});
trackMqtt(topic + "/period_min", function(message){
  dev[device_id + "/Период пробуждения"] = parseInt(message.value);
});
trackMqtt(topic + "/f0", function(message){
  dev[device_id + "/Вес горячей воды"] = parseInt(message.value);
});
trackMqtt(topic + "/f1", function(message){
  dev[device_id + "/Вес холодной воды"] = parseInt(message.value);
});


// с версии 1.0.0 (если включен в настройках Ватериуса флаг "homeassistant discovery"). Если выключен, Ватериус будет слать параметры в отдельных топиках.
trackMqtt(topic, function(message){
    try {
        var data = JSON.parse(message.value);
        dev[device_id + "/Горячая вода"] = data["ch0"];
        dev[device_id + "/Холодная вода"] = data["ch1"];
        dev[device_id + "/Период пробуждения"] = data["period_min"];
        dev[device_id + "/Вес горячей воды"] = data["f0"];
        dev[device_id + "/Вес холодной воды"] = data["f1"];
    } catch (e) {
        log("Failed to parse waterius data: " + e.message);
    }
});


// с версии >=0.11.0
defineRule("Установить Период пробуждения (мин)", {
    whenChanged: device_id + "/Период пробуждения",
    then: function (newValue) {
    publish(topic + "/period_min/set", newValue.toString());
    }
});
// с версии >=0.11.9
defineRule("Установить Горячую воду (м3)", {
    whenChanged: device_id + "/Горячая вода",
    then: function (newValue) {
    publish(topic + "/ch0/set", newValue.toString());
    }
});
// с версии >=0.11.9
defineRule("Установить Холодную воду (м3)", {
    whenChanged: device_id + "/Холодная вода",
    then: function (newValue) {
    publish(topic + "/ch1/set", newValue.toString());
    }
});
// с версии >=0.11.9
defineRule("Установить вес Горячей воды", {
    whenChanged: device_id + "/Вес горячей воды",
    then: function (newValue) {
    publish(topic + "/f0/set", newValue.toString());
    }
});
// с версии >=0.11.9
defineRule("Установить вес Холодной воды", {
    whenChanged: device_id + "/Вес холодной воды",
    then: function (newValue) {
    publish(topic + "/f1/set", newValue.toString());
    }
});


// TODO
//
// Определенно нужно добавить информацию об уровне заряда батареи!!!
// 

// Документация
//
// https://wirenboard.com/wiki/MQTT
// https://sprut.ai/article/put-ot-glupogo-chaynika-k-umnomu-redmond


