#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <DW1000Ng.hpp>
#include <DW1000NgUtils.hpp>
#include <DW1000NgRanging.hpp>
#include <DW1000NgRTLS.hpp>
#include <U8g2lib.h>
#include <Wire.h>
#include <PubSubClient.h>

const char* ssid = "gattini";
const char* password = "freud1988";
const char* mqtt_server = "192.168.0.10";
const char* mqttUser = "user";
const char* mqttPassword = "pass";
WiFiClient espClient;
PubSubClient client(espClient);
String pubTopic;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

const uint8_t PIN_RST = 16;
const uint8_t PIN_SS = 17;  // spi select pin
char EUI[] = "AA:BB:CC:DD:EE:FF:00:02";
uint16_t anchor_address = 2;
uint16_t next_anchor = 3;
float range_self;

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

frame_filtering_configuration_t ANCHOR_FRAME_FILTER_CONFIG = {
  false,
  false,
  true,
  false,
  false,
  false,
  false,
  false /* This allows blink frames */
};

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    String clientname = "ESP-" + String(WiFi.macAddress());
    if (client.connect(clientname.c_str(), mqttUser, mqttPassword)) {
    } else {
      Serial.println("mqtt error");
      delay(1000);
    }
  }
}

void initOTA() {
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname("UWB-ANCHOR-B");

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
  pubTopic = "devices/b/telemetry";
  client.setServer(mqtt_server, 1883);

  Serial.println(F("### DW1000Ng-arduino-ranging-anchorMain ###"));
  // initialize the driver
  DW1000Ng::initializeNoInterrupt(PIN_SS, PIN_RST);
  Serial.println(F("DW1000Ng initialized ..."));
  // general configuration
  DW1000Ng::applyConfiguration(DEFAULT_CONFIG);
  DW1000Ng::enableFrameFiltering(ANCHOR_FRAME_FILTER_CONFIG);
  DW1000Ng::setEUI(EUI);
  DW1000Ng::setPreambleDetectionTimeout(64);
  DW1000Ng::setSfdDetectionTimeout(273);
  DW1000Ng::setReceiveFrameWaitTimeoutPeriod(5000);
  DW1000Ng::setNetworkId(RTLS_APP_ID);
  DW1000Ng::setDeviceAddress(anchor_address);
  DW1000Ng::setAntennaDelay(16436);
  //DW1000Ng::setTXPower(0x0E082848L);
  DW1000Ng::enableDebounceClock();
  DW1000Ng::enableLedBlinking();
  DW1000Ng::setGPIOMode(MSGP0, LED_MODE);
  DW1000Ng::setGPIOMode(MSGP1, LED_MODE);
  //DW1000Ng::setGPIOMode(MSGP2, LED_MODE);
  //DW1000Ng::setGPIOMode(MSGP3, LED_MODE);

  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);
  u8g2.clearBuffer();
  u8g2.drawStr(0, 0, "ANCHOR B");

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
  RangeAcceptResult result = DW1000NgRTLS::anchorRangeAccept(NextActivity::RANGING_CONFIRM, next_anchor);
  if (result.success) {
    delay(2);
    range_self = result.range;
    String rangeString = "ANCHOR B - Range: ";
    rangeString += range_self;
    rangeString += " m";
    rangeString += "\t RX power: ";
    rangeString += DW1000Ng::getReceivePower();
    rangeString += " dBm";
    Serial.println(rangeString);

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_helvB14_tr);
    u8g2.drawStr(0, 0, "ANCHOR B");
    char buf[16];
    sprintf(buf, "d=%0.2fm", range_self);
    u8g2.drawStr(0, 16, buf);
    sprintf(buf, "%d", cnt++);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 64 - 10 - 1, buf);
    u8g2.sendBuffer();

    char temp[16];
    sprintf(temp, "%f", range_self);
    client.publish(pubTopic.c_str(), temp);
  }
  
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  ArduinoOTA.handle();
}
