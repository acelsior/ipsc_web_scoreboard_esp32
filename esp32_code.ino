#include <ESP32Time.h>

/*
   Based on 31337Ghost's reference code from https://github.com/nkolban/esp32-snippets/issues/385#issuecomment-362535434
   which is based on pcbreflux's Arduino ESP32 port of Neil Kolban's example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleScan.cpp
*/

/*
   Create a BLE server that will send periodic iBeacon frames.
   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create advertising data
   3. Start advertising.
   4. wait
   5. Stop advertising.
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLEBeacon.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "soc/rtc_wdt.h"


#define DEVICE_NAME "ESP32"
#define SERVICE_UUID "7A0247E7-8E88-409B-A959-AB5092DDB03E"
#define START_SIGNAL_CHARACTERISTIC_UUID "3C224D84-566D-4F13-8B1C-2117021FF1A2"
#define STOPPLATE_SIGNAL_CHARACTERISTIC_UUID "57B92756-3DF4-4038-B825-FC8E1C2FDB5B"
#define TIME_CORRECTION_CHARACTERISTIC_UUID "840A0941-55E9-44E4-BFFF-1C3C27BF6AF0"
#define SETTING_STORE_CHARACTERISTIC_UUID "798F2478-4C44-417F-BB6E-EE2A826CC17C"

//pin define
#define HX711_SCK 16
#define HX711_DT 4

#define ORANGE_INDICATOR 4
#define BLUE_INDICATOR 2
#define ANA_MIC_PIN 32 
#define DIG_MIC_PIN 33 

//setting
Preferences preferences;
// enum BuzzerWaveformType {
//   Sine,
//   Square,
//   Sawtooth,
//   Triangle
// };
int flashDuration = 3;
float minRandomTime = 1;
float maxRandomTime = 3;
int buzzerHertz = 500;
int buzzerWaveform = 1;

StaticJsonDocument<200> setting_dto;
char setting_dto_json_doc[100];
DeserializationError json_error;
const char *payload_room;
const char *payload_msg;

void updateSettingJSON() {
  setting_dto["flashDuration"] = flashDuration;
  setting_dto["minRandomTime"] = minRandomTime;
  setting_dto["maxRandomTime"] = maxRandomTime;
  setting_dto["buzzerHertz"] = buzzerHertz;
  setting_dto["buzzerWaveform"] = buzzerWaveform;
  serializeJson(setting_dto, setting_dto_json_doc);
  Serial.print("Setting json updated: ");
  Serial.println(setting_dto_json_doc);
}


BLEServer *pServer;
BLECharacteristic *startSignalCharacteristic;
BLECharacteristic *stopPlateCharacteristic;
BLECharacteristic *timeCorrectionCharacteristic;
BLECharacteristic *settingStoreCharacteristic;

bool deviceConnected = false;
ESP32Time rtc(28800);

long startEpoch = 0;
long startMicro = 0;

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    Serial.println("deviceConnected = true");
  };

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    Serial.println("deviceConnected = false");

    // Restart advertising to be visible and connectable again
    BLEAdvertising *pAdvertising;
    pAdvertising = pServer->getAdvertising();
    pAdvertising->start();
    Serial.println("iBeacon advertising restarted");
  }
};
class startSignalCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    Serial.println("start count");
    startEpoch = rtc.getLocalEpoch();
    startMicro = rtc.getMicros();
  }
  void onRead(BLECharacteristic *pCharacteristic) {
    Serial.println("start count");
    startEpoch = rtc.getLocalEpoch();
    startMicro = rtc.getMicros();
  }
};
class stopPlateCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();

    if (rxValue.length() > 0) {
      Serial.print("Received Value: ");
      for (int i = 0; i < rxValue.length(); i++) {
        Serial.print(rxValue[i]);
      }
      Serial.println();
    }
  }
};
class timeCorrectionCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();
    String tempStoreStr = "";
    int timeStamp;

    if (rxValue.length() > 0) {
      for (int i = 0; i < rxValue.length(); i++) {
        tempStoreStr += rxValue[i];
      }
      Serial.println(tempStoreStr);
      timeStamp = tempStoreStr.toInt();
      rtc.setTime(timeStamp);
    }
  }
};
class settingStoreCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    json_error = deserializeJson(setting_dto, value);
    if (!json_error) {
      flashDuration = setting_dto["flashDuration"];
      minRandomTime = setting_dto["minRandomTime"];
      maxRandomTime = setting_dto["maxRandomTime"];
      buzzerHertz = setting_dto["buzzerHertz"];
      buzzerWaveform = setting_dto["buzzerWaveform"];
      Serial.print("flashDuration: ");
      Serial.println(flashDuration);
      Serial.print("minRandomTime: ");
      Serial.println(minRandomTime);
      Serial.print("maxRandomTime: ");
      Serial.println(maxRandomTime);
      Serial.print("buzzerHertz: ");
      Serial.println(buzzerHertz);
      Serial.print("buzzerWaveform: ");
      Serial.println(buzzerWaveform);
      preferences.begin("stopplate", false);
      preferences.putInt("flashDuration", flashDuration);
      preferences.putFloat("minRandomTime", minRandomTime);
      preferences.putFloat("maxRandomTime", maxRandomTime);
      preferences.putInt("buzzerHertz", buzzerHertz);
      preferences.putInt("buzzerWaveform", buzzerWaveform);
      preferences.end();
      updateSettingJSON();
      settingStoreCharacteristic->setValue(setting_dto_json_doc);
    } else {
      Serial.println("Error");
      int flashDuration = 3;
      float minRandomTime = 1;
      float maxRandomTime = 3;
      int buzzerHertz = 500;
      int buzzerWaveform = 1;
      preferences.begin("stopplate", false);
      preferences.putInt("flashDuration", flashDuration);
      preferences.putFloat("minRandomTime", minRandomTime);
      preferences.putFloat("maxRandomTime", maxRandomTime);
      preferences.putInt("buzzerHertz", buzzerHertz);
      preferences.putInt("buzzerWaveform", buzzerWaveform);
      preferences.end();
      updateSettingJSON();
      settingStoreCharacteristic->setValue(setting_dto_json_doc);
    }
  }
};

void init_service() {
  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  stopPlateCharacteristic = pService->createCharacteristic(
    STOPPLATE_SIGNAL_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE);
  stopPlateCharacteristic->setCallbacks(new stopPlateCallbacks());
  stopPlateCharacteristic->addDescriptor(new BLE2902());

  timeCorrectionCharacteristic = pService->createCharacteristic(
    TIME_CORRECTION_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  timeCorrectionCharacteristic->setCallbacks(new timeCorrectionCallbacks());
  timeCorrectionCharacteristic->addDescriptor(new BLE2902());

  startSignalCharacteristic = pService->createCharacteristic(
    START_SIGNAL_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  startSignalCharacteristic->setCallbacks(new startSignalCallbacks());
  startSignalCharacteristic->addDescriptor(new BLE2902());

  settingStoreCharacteristic = pService->createCharacteristic(
    SETTING_STORE_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  settingStoreCharacteristic->setCallbacks(new settingStoreCallback());
  settingStoreCharacteristic->addDescriptor(new BLE2902());
  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
}


void hitHandler(int val) {
  Serial.println("HIT");
  if (digitalRead(ORANGE_INDICATOR) == 1) {
    digitalWrite(ORANGE_INDICATOR, LOW);
    digitalWrite(BLUE_INDICATOR, HIGH);
  } else {
    digitalWrite(ORANGE_INDICATOR, HIGH);
    digitalWrite(BLUE_INDICATOR, LOW);
  }
  if (deviceConnected) {
    Serial.print("*** NOTIFY: ");
    Serial.print(val);
    Serial.print("***\n");
    long totalTime = rtc.getLocalEpoch() - startEpoch;
    int hr = (totalTime % 86400L) / 3600;
    int min = (totalTime % 3600) / 60;
    int sec = (totalTime % 60);
    int nowMicro = rtc.getMicros();
    int mil;
    if (nowMicro > startMicro) {
      mil = nowMicro - startMicro;
    } else {
      mil = startMicro - rtc.getMicros();
    }
    // String timeStr = rtc.getDateTime();
    String timeStr = "{";
    timeStr += "\"hours\":";
    timeStr += hr;
    timeStr += ",";
    timeStr += "\"minutes\":";
    timeStr += min;
    timeStr += ",";
    timeStr += "\"seconds\":";
    timeStr += sec;
    timeStr += ",";
    timeStr += "\"milliseconds\":";
    timeStr += mil;
    timeStr += "}";
    Serial.println(timeStr);
    std::string timeStdStr(timeStr.c_str(), timeStr.length());
    stopPlateCharacteristic->setValue(timeStdStr);
    stopPlateCharacteristic->notify();
  }
}

void setup() {
  
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  rtc_wdt_protect_off();
  rtc_wdt_disable();


  Serial.begin(115200);
  Serial.println();
  Serial.println("Initializing...");
  delay(1000);
  // Serial.flush();
  // esp_log_level_set("*", ESP_LOG_ERROR);        // set all components to ERROR level

  pinMode(BLUE_INDICATOR, OUTPUT);
  pinMode(ORANGE_INDICATOR, OUTPUT);
  digitalWrite(BLUE_INDICATOR, HIGH);
  digitalWrite(ORANGE_INDICATOR, HIGH);

  BLEDevice::init(DEVICE_NAME); //force reset here

  digitalWrite(BLUE_INDICATOR, LOW);
  digitalWrite(ORANGE_INDICATOR, LOW);

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  init_service();
  // initPlate();

  preferences.begin("stopplate", false);
  if (preferences.getInt("flashDuration", -1) == -1) {
    preferences.putInt("flashDuration", flashDuration);
    Serial.println("null flash");
  } else {
    flashDuration = preferences.getInt("flashDuration", -1);
  };
  if (preferences.getFloat("minRandomTime", -1) == -1) {
    preferences.putFloat("minRandomTime", minRandomTime);
  } else {
    minRandomTime = preferences.getFloat("minRandomTime", -1);
  };
  if (preferences.getFloat("maxRandomTime", -1) == -1) {
    preferences.putFloat("maxRandomTime", maxRandomTime);
  } else {
    maxRandomTime = preferences.getFloat("maxRandomTime", -1);
  };
  if (preferences.getInt("buzzerHertz", -1) == -1) {
    preferences.putInt("buzzerHertz", buzzerHertz);
  } else {
    buzzerHertz = preferences.getInt("buzzerHertz", -1);
  };
  if (preferences.getInt("buzzerWaveform", -1) == -1) {
    preferences.putInt("buzzerWaveform", buzzerWaveform);
  } else {
    buzzerWaveform = preferences.getInt("buzzerWaveform", -1);
  };
  preferences.end();

  Serial.print("flashDuration: ");
  Serial.println(flashDuration);
  Serial.print("minRandomTime: ");
  Serial.println(minRandomTime);
  Serial.print("maxRandomTime: ");
  Serial.println(maxRandomTime);
  Serial.print("buzzerHertz: ");
  Serial.println(buzzerHertz);
  Serial.print("buzzerWaveform: ");
  Serial.println(buzzerWaveform);

  updateSettingJSON();
  settingStoreCharacteristic->setValue(setting_dto_json_doc);

  Serial.println(setting_dto_json_doc);

  Serial.println("iBeacon + service defined and advertising!");
}

int stopplatePressureChangeThreshold = 50;
int stopplatePressureErrorLimit = 6000;
int stopplateTriggerCooldown = 200;

long lastTrigger;
void loop() {
  int micVal = analogRead(ANA_MIC_PIN);
  // Serial.print(analogRead(DIG_MIC_PIN));
  // Serial.print(",");
  // Serial.println(analogRead(ANA_MIC_PIN));
  delay(10);
  if (micVal >= 1200 && millis() - lastTrigger >= stopplateTriggerCooldown) {
    lastTrigger = millis();
    hitHandler(micVal);
  }
  // Serial.print(currentPressure);
  // Serial.print(",   ");
  // Serial.println(prevPressure);
  // && currentPressure <= stopplatePressureErrorLimit
  // if (currentPressure - prevPressure >= stopplatePressureChangeThreshold) {
  //   hitHandler(currentPressure);
  // }

  // prevPressure = currentPressure;
  // delay(5);
}
