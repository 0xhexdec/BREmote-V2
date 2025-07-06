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
    }

    pCharacteristic->setValue(file.readString());
    file.close();
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
  }
};

void handleBLE() {
    scroll3Digits(LET_B, LET_L, LET_E, 200);
    bleInit();
      while(1) 
      {
        if(deviceConnected) {
            scroll3Digits(5, LET_U, LET_C, 200);
        } else {
            scroll3Digits(LET_B, LET_L, LET_E, 200);
        }
      }
}

void bleInit() {
  Serial.println("Starting BLE...");

  // TODO: Add Field to SPIFFS to persist this and make it configurable
  BLEDevice::init("BREmote - Transmitter");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());
  BLEService *pService = pServer->createService(TRANSMITTER_SERVICE_UUID);
  BLECharacteristic *pCharacteristic =
    pService->createCharacteristic(TRANSMITTER_CONFIG_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);

  pCharacteristic->setCallbacks(new TransmitterConfigCallback());
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(TRANSMITTER_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x08);
  pAdvertising->setMaxPreferred(0x08);
  BLEDevice::startAdvertising();
  Serial.println("BLE started.");
}