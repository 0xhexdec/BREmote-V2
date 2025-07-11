// BLE
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define TRANSMITTER_SERVICE_UUID                    "de50d6f3-5593-48b7-8173-a10bc1d4a3aa"
#define TRANSMITTER_CONFIG_CHARACTERISTIC_UUID      "bcbfc8b5-3b55-41ee-8085-75f3b42055dd"
#define TRANSMITTER_LIVE_DATA_CHARACTERISTIC_UUID   "beabca38-64aa-4091-8a30-2065ec499126"

#define OTA_SERVICE_UUID                            "9fe8f1ff-af90-48a7-aa20-c6630339e2e3"
#define OTA_STATUS_CHARACTERISTIC_UUID              "fa28c7d2-2b49-4e39-aca0-b37f02497a67"
#define OTA_DATA_CHARACTERISTIC_UUID                "1337bf8c-aa19-4ea7-9832-dee0a713357e"

#define OTA_VERSION 1
#define FIRMWARE_VERSION 2

#define OTA_STATUS_IDLE 0
#define OTA_STATUS_IN_PROGRESS 1
#define OTA_STATUS_DONE 2
#define OTA_STATUS_ABORT 3
#define OTA_STATUS_ERROR 4
#define OTA_STATUS_REBOOT 5

#define OTA_STATUS_ERROR_OK 0
#define OTA_STATUS_ERROR_DISCONNECT 1
#define OTA_STATUS_ERROR_WRITE 2
#define OTA_STATUS_ERROR_NO_SPACE 3
#define OTA_STATUS_ERROR_NOT_STARTED 4
#define OTA_STATUS_ERROR_UNSUPPORTED_VERSION 5
#define OTA_STATUS_ERROR_BINARY_SIZE_DOES_NOT_MATCH 6
#define OTA_STATUS_ERROR_UNEXPECTED_PAYLOAD_TYPE 7
#define OTA_STATUS_ERROR_UKNOWN_COMMAND 8
#define OTA_STATUS_ERROR_UKNOWN_DATA 9
#define OTA_STATUS_ERROR_UPDATE_FAILED 10
#define OTA_STATUS_ERROR_UPDATE_ALREADY_RUNNING 11

bool deviceConnected = false;

class TransmitterConfigCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic, esp_ble_gatts_cb_param_t *param) {
    Serial.println("BLE write request");
    String value = pCharacteristic->getValue();
    
    if (value.length() > 0) {
      Serial.println("*********");
      Serial.print("New value: ");
      for (int i = 0; i < value.length(); i++) {
        Serial.print(value[i]);
      }

      Serial.println();
      Serial.println("*********");
      serSetConf(value);
    }
  }
  void onRead(BLECharacteristic *pCharacteristic, esp_ble_gatts_cb_param_t *param) {
    Serial.println("BLE read request");
    File file = SPIFFS.open(CONF_FILE_PATH, FILE_READ);
    if (!file) {
      Serial.println("Failed to open file for reading");
      pCharacteristic->setValue("error reading config");
    } else {
      pCharacteristic->setValue(file.readString());
      file.close();
    }
  }
};

class LiveDataCallback : public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic *pCharacteristic, esp_ble_gatts_cb_param_t *param) {
    Serial.println("BLE live data request");
    pCharacteristic->setValue("Not implemented yet");
  }
};

struct OtaStatus {
  uint8_t otaVersion = OTA_VERSION;
  uint8_t firmwareVersion = FIRMWARE_VERSION;
  uint8_t statusCode = OTA_STATUS_IDLE;
  uint8_t errorCode = 0;
  size_t binarySize = 0;
};

OtaStatus otaStatus;

class OTAStatusCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic, esp_ble_gatts_cb_param_t *param) {
    Serial.println("OTAStatusCallback write request");
    // Serial.print("otaStatus: ");
    // Serial.println(otaStatus);
    uint8_t* data = pCharacteristic->getData();
    size_t length = pCharacteristic->getLength();

    if (length < 5) {
      Serial.print("Unsupported Data with size ");
      Serial.println(length);
      otaStatus.statusCode = OTA_STATUS_ERROR;
      otaStatus.errorCode = OTA_STATUS_ERROR_UKNOWN_DATA;
      return;
    }

    // doing some dirty pointer shit...
    OtaStatus* status = (OtaStatus*) data;
    if (status->otaVersion != OTA_VERSION) {
      Serial.print("Unsupported OTA version ");
      Serial.println(otaStatus.otaVersion);
      otaStatus.statusCode = OTA_STATUS_ERROR;
      otaStatus.errorCode = OTA_STATUS_ERROR_UNSUPPORTED_VERSION;
      return;
    }
    if (status->statusCode == OTA_STATUS_IN_PROGRESS ) {
      if (otaStatus.statusCode != OTA_STATUS_IDLE) {
        Serial.println("Update already running! Cancel or proceed.");
        otaStatus.statusCode = OTA_STATUS_ERROR;
        otaStatus.errorCode = OTA_STATUS_ERROR_UPDATE_ALREADY_RUNNING;
        return;
      }
      if(status->firmwareVersion <= otaStatus.firmwareVersion) {
        Serial.println("the new firmware is the same or older compared to the current one");
      }
      // gettin' dirtier!
      if (startOtaUpdate(status->binarySize)) {
        Serial.print("Starting OTA Update with ");
        Serial.print(status->binarySize);
        Serial.println(" bytes");
        otaStatus.binarySize = status->binarySize;
        otaStatus.statusCode = OTA_STATUS_IN_PROGRESS;
        otaStatus.errorCode = OTA_STATUS_ERROR_OK;
        return;
      } else {
        Serial.print("Failed to start OTA Update with ");
        Serial.print(status->binarySize);
        Serial.println(" bytes");
        otaStatus.statusCode = OTA_STATUS_ERROR;
        otaStatus.errorCode = OTA_STATUS_ERROR_NO_SPACE;
        return;
      }
    } else if (status->statusCode == OTA_STATUS_DONE) {
      if(otaStatus.statusCode == OTA_STATUS_IN_PROGRESS) {
        if (endOtaUpdate()) {
          Serial.println("Ota Update Done, reboot now");
          otaStatus.binarySize = 0;
          otaStatus.statusCode = OTA_STATUS_DONE;
          otaStatus.errorCode = OTA_STATUS_ERROR_OK;
          return;
        } else {
          Serial.println("OTA Update Failed!");
          otaStatus.binarySize = 0;
          otaStatus.statusCode = OTA_STATUS_ERROR;
          otaStatus.errorCode = OTA_STATUS_ERROR_NO_SPACE;
          return;
        }
      } else {
        Serial.println("OTA Update not in progress, cant complete");
      }
    } else if (status->statusCode == OTA_STATUS_ABORT) {
      Serial.println("Abort current ota update");
      abort();
      otaStatus.binarySize = 0;
      otaStatus.statusCode = OTA_STATUS_ABORT;
      otaStatus.errorCode = OTA_STATUS_ERROR_OK;
    } else if (status->statusCode == OTA_STATUS_REBOOT) {
      Serial.println("Reboot device...");
      otaStatus.statusCode = OTA_STATUS_REBOOT;
      rebootCounter = 50;
    }
    Serial.print("Unknown command ");
    Serial.println(status->statusCode);
    otaStatus.statusCode = OTA_STATUS_ERROR;
    otaStatus.errorCode = OTA_STATUS_ERROR_UKNOWN_COMMAND;
  }
  void onRead(BLECharacteristic *pCharacteristic, esp_ble_gatts_cb_param_t *param) {
    Serial.println("OTAStatusCallback read request");
    Serial.print("otaVersion: ");
    Serial.println(otaStatus.otaVersion);
    Serial.print("firmwareVersion: ");
    Serial.println(otaStatus.firmwareVersion);
    Serial.print("statusCode: ");
    Serial.println(otaStatus.statusCode);
    Serial.print("errorCode: ");
    Serial.println(otaStatus.errorCode);
    Serial.print("binarySize: ");
    Serial.println(otaStatus.binarySize);
    // send status struct
    uint8_t status[8];
    memcpy(status, (uint8_t*)&otaStatus, 8);
    pCharacteristic->setValue(status, 8);
    if (otaStatus.statusCode == OTA_STATUS_ERROR) {
      otaStatus.statusCode = OTA_STATUS_IDLE;
      otaStatus.errorCode = OTA_STATUS_ERROR_OK;
    }
    // pCharacteristic
  }
};

class OTADataCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic, esp_ble_gatts_cb_param_t *param) {
    Serial.println("OTADataCallback write request");
    if (otaStatus.statusCode != OTA_STATUS_IN_PROGRESS) {
      Serial.println("OTA Update not in progress, can't write data.");
      otaStatus.statusCode = OTA_STATUS_ERROR;
      otaStatus.errorCode = OTA_STATUS_ERROR_NOT_STARTED;
      return;
    }
    uint8_t* data = pCharacteristic->getData();
    size_t length = pCharacteristic->getLength();
    if (writeData(data, length)) {
      Serial.println("Data written successfully");
      otaStatus.statusCode = OTA_STATUS_IN_PROGRESS;
      otaStatus.errorCode = OTA_STATUS_ERROR_OK;
      return;
    } else {
      Serial.println("Data writing failed");
      otaStatus.statusCode = OTA_STATUS_ERROR;
      otaStatus.errorCode = OTA_STATUS_ERROR_UPDATE_FAILED;
      return;
    }
  }
};

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    // dummy
    deviceConnected = true;
    Serial.println("Connected");
  }

  void onDisconnect(BLEServer *pServer) {
    // dummy
    deviceConnected = false;
    Serial.println("Disconnected");
    // TODO: if ota in progress -> finish ota with error
    BLEDevice::startAdvertising();
  }
  void onMtuChanged(BLEServer *pServer, esp_ble_gatts_cb_param_t *param) {
    Serial.print("MTU changed to: ");
    Serial.println(param->mtu.mtu);
  }
};

void handleBLE() {
  bleInit();
  while(1) 
  {
    switch (otaStatus.statusCode) {
    case OTA_STATUS_IN_PROGRESS:
      scroll3Digits(0, LET_T, LET_A, 200);
      break;
    case OTA_STATUS_REBOOT:
      scroll3Digits(LET_R, 5, LET_T, 200);
      scroll3Digits(LET_R, 5, LET_T, 200);
      scroll3Digits(LET_R, 5, LET_T, 200);
      ESP.restart();
      break;
    case OTA_STATUS_IDLE:
      if(!deviceConnected) {
        scroll3Digits(LET_B, LET_L, LET_E, 200);
      } else {
        displayDigits(BLE_1, BLE_2);
        delay(200);
      }
      break;
    }
  }
}

void bleInit() {
  Serial.println("Starting BLE...");

  // TODO: Add Field to SPIFFS to persist this and make it configurable
  BLEDevice::init("BREmote V2 Tx");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());
  BLEService *pService = pServer->createService(TRANSMITTER_SERVICE_UUID);
  BLECharacteristic *pConfigChar =
    pService->createCharacteristic(TRANSMITTER_CONFIG_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);

  pConfigChar->setCallbacks(new TransmitterConfigCallback());

  BLECharacteristic *pLiveDataChar =
    pService->createCharacteristic(TRANSMITTER_LIVE_DATA_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ);

  pLiveDataChar->setCallbacks(new LiveDataCallback());
  pService->start();

  BLEService *pOtaService = pServer->createService(OTA_SERVICE_UUID);
  BLECharacteristic *pOtaStatusChar =
    pOtaService->createCharacteristic(OTA_STATUS_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  pOtaStatusChar->setCallbacks(new OTAStatusCallback());

  BLECharacteristic *pOtaDataChar =
    pOtaService->createCharacteristic(OTA_DATA_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE);
  pOtaDataChar->setCallbacks(new OTADataCallback());

  pOtaService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(TRANSMITTER_SERVICE_UUID);
  pAdvertising->addServiceUUID(OTA_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x08);
  pAdvertising->setMaxPreferred(0x08);
  BLEDevice::startAdvertising();
  Serial.println("BLE started.");
}