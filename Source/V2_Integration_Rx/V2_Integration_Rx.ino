#include "BREmote_V2_Rx.h"

SX1262 radio = new Module(P_LORA_NSS, P_LORA_DIO, P_LORA_RST, P_LORA_BUSY);
Adafruit_AW9523 aw;
Ticker ticksrc;
TinyGPSPlus gps;

void setup()
{
  enterSetup();

  Wire.begin(P_I2C_SDA, P_I2C_SCL); //SDA, SCL
  Wire.setClock(400000); // Set to 400 kHz
  startupAW();

  initSPIFFS();
  getConfFromSPIFFS();

  startupRadio();
  radio.setPacketReceivedAction(packetReceived);
  radio.implicitHeader(6);
  radio.startReceive();

  checkButtons();

  rxIsrState = 1;

  initRMT();

  Serial1.begin(115200, SERIAL_8N1, P_U1_RX, P_U1_TX);
  
  aw.digitalWrite(AP_EN_PWM0, 1);
  aw.digitalWrite(AP_EN_PWM1, 1);

  triggerReceiveSemaphore = xSemaphoreCreateBinary();
  loopTaskHandle = xTaskGetCurrentTaskHandle();
  
  //Runs every 10ms to generate both PWM signals, high prio
  xTaskCreatePinnedToCore(generatePWM, "Generate_PWM_10ms", 2048, NULL, 10, &generatePWMHandle, 0);
  //Runs upon RF interrupt and reads packet & responds, medium-high prio
  xTaskCreatePinnedToCore(triggeredReceive, "RF_ReceiveTask_triggered", 2048, NULL, 5, &triggeredReceiveHandle, 0);

  //Checks if there is connection and blinks LED, low prio
  xTaskCreatePinnedToCore(checkConnStatus, "Check_conn_staus_200ms", 2048, NULL, 2, &checkConnStatusHandle, 0);

  configureGPS();

  exitSetup();
  PWM_active = 1;
  
  //pinMode(P_UBAT_MEAS, OUTPUT);
}

unsigned long loop_timer = 0;

void loop()
{
  checkSerial();

  if(millis()-loop_timer > 1000)
  {
    loop_timer = millis();
    if(usrConf.wet_det_active)
    {
      checkWetness();
    }

    if(usrConf.gps_en)
    {
      getGPSLoop();
    }

    if(usrConf.data_src == 1)
    {
      getUbatLoop();
    }
    else if(usrConf.data_src == 2)
    {
      getVescLoop();
    }
  }

  //telemetry.foil_speed = (millis()/1000) % 100;

  vTaskDelay(pdMS_TO_TICKS(10));
}

