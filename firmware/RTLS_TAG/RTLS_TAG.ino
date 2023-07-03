/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/* 
 * StandardRTLSTag_TWR.ino
 * 
 * This is an example tag in a RTLS using two way ranging ISO/IEC 24730-62_2013 messages
 */
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <DW1000Ng.hpp>
#include <DW1000NgUtils.hpp>
#include <DW1000NgTime.hpp>
#include <DW1000NgConstants.hpp>
#include <DW1000NgRanging.hpp>
#include <DW1000NgRTLS.hpp>
#include <U8g2lib.h>
#include <Wire.h>

const char* ssid = "gattini";
const char* password = "freud1988";


U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

const uint8_t PIN_RST = 16;
const uint8_t PIN_SS = 17;  // spi select pin

// Extended Unique Identifier register. 64-bit device identifier. Register file: 0x01
char EUI[] = "AA:BB:CC:DD:EE:FF:00:00";

volatile uint32_t blink_rate = 200;

device_configuration_t DEFAULT_CONFIG = {
  false,
  true,
  true,
  true,
  false,
  SFDMode::STANDARD_SFD,
  Channel::CHANNEL_5,
  DataRate::RATE_850KBPS,
  PulseFrequency::FREQ_16MHZ,
  PreambleLength::LEN_256,
  PreambleCode::CODE_3
};

frame_filtering_configuration_t TAG_FRAME_FILTER_CONFIG = {
  false,
  false,
  true,
  false,
  false,
  false,
  false,
  false
};

sleep_configuration_t SLEEP_CONFIG = {
  false,  // onWakeUpRunADC   reg 0x2C:00
  false,  // onWakeUpReceive
  false,  // onWakeUpLoadEUI
  true,   // onWakeUpLoadL64Param
  true,   // preserveSleep
  true,   // enableSLP    reg 0x2C:06
  false,  // enableWakePIN
  true    // enableWakeSPI
};


void initOTA() {
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname("UWB-ANCHOR-TAG");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else  // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_helvB14_tr);
      char buf[16];
      sprintf(buf, "OTA: %u%%\r", (progress / (total / 100)));
      u8g2.drawStr(0, 0, buf);
      u8g2.sendBuffer();
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}


void setup() {
  // DEBUG monitoring
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  initOTA();

  Serial.println(F("### DW1000Ng-arduino-ranging-tag ###"));
  // initialize the driver
  DW1000Ng::initializeNoInterrupt(PIN_SS, PIN_RST);
  // general configuration
  DW1000Ng::applyConfiguration(DEFAULT_CONFIG);
  DW1000Ng::enableFrameFiltering(TAG_FRAME_FILTER_CONFIG);
  DW1000Ng::setEUI(EUI);
  DW1000Ng::setNetworkId(RTLS_APP_ID);
  DW1000Ng::setAntennaDelay(16436);
  DW1000Ng::applySleepConfiguration(SLEEP_CONFIG);
  DW1000Ng::setPreambleDetectionTimeout(15);
  DW1000Ng::setSfdDetectionTimeout(273);
  DW1000Ng::setReceiveFrameWaitTimeoutPeriod(2000);
  //DW1000Ng::setTXPower(0x0E082848L);
  DW1000Ng::enableDebounceClock();
  DW1000Ng::enableLedBlinking();
  DW1000Ng::setGPIOMode(MSGP0, LED_MODE);
  DW1000Ng::setGPIOMode(MSGP1, LED_MODE);
  DW1000Ng::setGPIOMode(MSGP2, LED_MODE);
  DW1000Ng::setGPIOMode(MSGP3, LED_MODE);


  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);

  u8g2.clearBuffer();
  u8g2.drawStr(0, 0, "TAG");

  char msg[128];
  DW1000Ng::getPrintableDeviceIdentifier(msg);
  Serial.print("Device ID: ");
  Serial.println(msg);
  u8g2.drawStr(0, 12, msg);

  DW1000Ng::getPrintableExtendedUniqueIdentifier(msg);
  Serial.print("Unique ID: ");
  Serial.println(msg);
  u8g2.drawStr(0, 24, msg);

  DW1000Ng::getPrintableNetworkIdAndShortAddress(msg);
  Serial.print("Network ID & Device Address: ");
  Serial.println(msg);
  u8g2.drawStr(0, 36, msg);

  DW1000Ng::getPrintableDeviceMode(msg);
  Serial.print("Device mode: ");
  Serial.println(msg);
  u8g2.sendBuffer();
}
uint32_t cnt = 0;
void loop() {
  //DW1000Ng::deepSleep();
  delay(blink_rate);
  //DW1000Ng::spiWakeup();
  //DW1000Ng::setEUI(EUI);

  RangeInfrastructureResult res = DW1000NgRTLS::tagTwrLocalize(1600);
  if (res.success) {
    blink_rate = res.new_blink_rate;

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_helvB14_tr);
    u8g2.drawStr(0, 0, "TAG");
    char buf[16];
    sprintf(buf, "rate=%dms", blink_rate);
    u8g2.drawStr(0, 16, buf);
    sprintf(buf, "%d", cnt++);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 64 - 10 - 1, buf);
    u8g2.sendBuffer();
  }

  ArduinoOTA.handle();
}
