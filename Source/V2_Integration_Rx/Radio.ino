void startupRadio()
{
  Serial.print("Starting Radio...");

  SPI.begin(P_SPI_SCK, P_SPI_MISO, P_SPI_MOSI);

  if(usrConf.rf_power < -9 || usrConf.rf_power > 22)
  {
    Serial.println("Error, invalid transmit power");
    while(1) blinkErr(2, AP_L_BIND);
  }

  Serial.print(" Power: ");
  Serial.print(usrConf.rf_power);

  int state;

  if(usrConf.radio_preset == 1)
  {
    Serial.print(" Region: EU868");
    //869.4-869.65MHz, 10%TOA, 500mW
    //Checked allowed in: EU, Switzerland
                        //          5..12 5..8                                  -9..22             >=1
                        //fc     bw    sf cr                                      pwr              pre tcxo  ldo
    state = radio.begin(869.525, 250.0, 6, 7, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, usrConf.rf_power, 8, 1.8, false);
  }
  else if(usrConf.radio_preset == 2)
  {
    //Reserved for US
    Serial.println("Error, unsupported HF setting");
    while(1) blinkErr(3, AP_L_BIND);
  }
  else if (usrConf.radio_preset == 3)
  {
    //Reserverd for other country
    Serial.println("Error, unsupported HF setting");
    while(1) blinkErr(3, AP_L_BIND);
  }
  else
  {
    //Invalid
    Serial.println("Error, unsupported HF setting");
    while(1) blinkErr(3, AP_L_BIND);
  }

  //radio.setCurrentLimit(60.0);
  radio.setDio2AsRfSwitch(true);
  radio.implicitHeader(4);
  radio.setCRC(0);
  radio.setRxBandwidth(250);

  Serial.print(" TOA: ");
  Serial.print(radio.getTimeOnAir(4));

  if (state == RADIOLIB_ERR_NONE) 
  {
    Serial.println(" Done");
  } 
  else 
  {
    Serial.print(" Failed, code: ");
    Serial.println(state);
    while(1) blinkErr(3, AP_L_BIND);
  }
}

void ICACHE_RAM_ATTR packetReceived(void) 
{
  if(rxIsrState && !rfInterrupt)
  {
    rfInterrupt = true;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(triggerReceiveSemaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
  else
  {
    rfInterrupt = true;
  }
}

// Function for waiting node to pair
bool waitForPairing() 
{
  usrConf.paired = false;
  
  // First check for address conflicts
  if (0)//!checkAndAdjustAddress()) 
  {
    return false;  // Too many conflicts
  }
  
  uint8_t responsePacket[8];
  unsigned long startTime = millis();

  while (millis() - startTime < PAIRING_TIMEOUT) 
  {
    radio.implicitHeader(5);
    radio.startReceive();
    rxprintln("Waiting for pairing packet...");
    uint8_t buffer[15];
    
    while(!rfInterrupt && millis() - startTime < PAIRING_TIMEOUT) blinkBind(2);
    delay(10);
    if (rfInterrupt && radio.readData(buffer, 15) == RADIOLIB_ERR_NONE) 
    {
      rfInterrupt = false;
      rxprintln("Received response");
      #ifdef DEBUG_RX
      printHexArray(buffer, 15);
      #endif

      if (buffer[0] == 0xAB) 
      {
        rxprintln("1st byte matches");
        // Verify CRC of received packet directly
        uint8_t receivedCRC = buffer[4];
        uint8_t calculatedCRC = esp_crc8(buffer, 4);
        
        if (receivedCRC == calculatedCRC) 
        {
          rxprintln("CRC ok");
          // Store potential partner's address
          uint8_t temp_addr[3];
          memcpy(temp_addr, buffer + 1, 3);
          
          // Prepare response packet
          responsePacket[0] = 0xBA;
          memcpy(responsePacket + 1, temp_addr, 3);
          memcpy(responsePacket + 4, usrConf.own_address, 3);
          
          // Calculate CRC;
          responsePacket[7] = esp_crc8(responsePacket, 7);
          
          delay(100);
          rxprintln("Sending response: ");
          #ifdef DEBUG_RX
          printHexArray(responsePacket, 8);
          #endif
          // Send response
          radio.implicitHeader(8);
          radio.startTransmit(responsePacket, 8);
          delay(10);
          radio.startReceive();
          rfInterrupt = false;

          while (millis() - startTime < PAIRING_TIMEOUT) 
          {
            while(!rfInterrupt && millis() - startTime < PAIRING_TIMEOUT) delay(10);
            delay(10);
            if (rfInterrupt && radio.readData(buffer, 15) == RADIOLIB_ERR_NONE) 
            {
              rfInterrupt = false;
              rxprintln("Received response");
              if (buffer[0] == 0xAC && memcmp(buffer + 1, usrConf.own_address, 3) == 0) 
              {
                rxprintln("Address matches");
                // Verify CRC of final packet directly
                receivedCRC = buffer[7];
                calculatedCRC = esp_crc8(buffer, 7);
                
                if (receivedCRC == calculatedCRC) 
                {
                  rxprintln("CRC ok, pairing success");
                  // Save partner's address
                  memcpy(usrConf.dest_address, buffer + 4, 3);
                  usrConf.paired = true;
                  saveConfToSPIFFS(usrConf);
                  aw.digitalWrite(AP_L_BIND, LOW);
                  return true;
                }
              }
            }
            else rfInterrupt = false;
          }
        }
      }
    }
    else rfInterrupt = false;
  }  
  return false;
}

void triggeredReceive(void *parameter) {
  while (1)
  {
    // Wait for semaphore from ISR
    if (xSemaphoreTake(triggerReceiveSemaphore, portMAX_DELAY) == pdTRUE) 
    {
      //digitalWrite(P_UBAT_MEAS, HIGH);

      uint8_t rcvArray[6];
      if (radio.readData(rcvArray, 6) == RADIOLIB_ERR_NONE) 
      {

        rxprint("Received packet: ");
        #ifdef DEBUG_RX
        printHexArray(rcvArray,6);
        #endif

        if (memcmp(rcvArray, usrConf.own_address, 3) == 0) 
        {
          rxprintln("Address matches");
          
          if (rcvArray[5] == esp_crc8(rcvArray, 5)) 
          {
            last_packet = millis();
            rxprintln("CRC ok");
            rxprint("RSSI: ");
            rxprint(radio.getRSSI());
            rxprint(", SNR: ");
            rxprintln(radio.getSNR());

            thr_received = rcvArray[3];
            steering_received =  rcvArray[4];

            telemetry.link_quality = getLinkQuality(radio.getRSSI(), radio.getSNR());

            rxprintln("Sending response");

            uint8_t sendArray[6];

            memcpy(sendArray, usrConf.dest_address, 3);

            uint8_t* ptr = (uint8_t*)&telemetry;
            
            sendArray[3] = telemetry_index;
            
            sendArray[4] = ptr[telemetry_index];

            telemetry_index++;
            if(telemetry_index >= sizeof(TelemetryPacket))
            {
              telemetry_index = 0;
            }

            sendArray[5] = esp_crc8(sendArray, 5);

            #ifdef DEBUG_RX
            printHexArray(sendArray, 6);
            #endif

            vTaskDelay(pdMS_TO_TICKS(10));
            //digitalWrite(P_UBAT_MEAS, HIGH);
            radio.implicitHeader(6);
            radio.startTransmit(sendArray, 6);
            //digitalWrite(P_UBAT_MEAS, LOW);
            //Wait for packet to be fully sent
            vTaskDelay(pdMS_TO_TICKS(10));
            radio.implicitHeader(6);
            radio.startReceive();
          }
        }
      }
      else
      {
        rxprintln("Rx err");
      }
      radio.startReceive();
      rfInterrupt = false;
      //digitalWrite(P_UBAT_MEAS, LOW);
    }
  }
}

int getLinkQuality(float rssi, float snr) {
  // Normalize RSSI: Expected range (-130 dBm to -50 dBm)
  int rssiScore = constrain(map(rssi, -100, -50, 0, 10), 0, 10);

  // Normalize SNR: Typical range (-20 dB to +10 dB)
  int snrScore = constrain(map(snr, -10, 10, 0, 10), 0, 10);

  // Weighted average (adjust weights as needed)
  float combinedScore = (0.7 * rssiScore) + (0.3 * snrScore);

  // Convert to integer and ensure it's in range 0-10
  return constrain(round(combinedScore), 0, 10);
}