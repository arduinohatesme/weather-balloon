/*
 * BalloonController.cpp - Controller for Launchpad Incubator's weather balloon project
 * Created by Aidan McMillan, June 29, 2026.
 * Released into the public domain.
 */
#include "BalloonController.h";
#include "Arduino.h";

#include <Wire.h>
#include <WiFi.h>
#include <math.h>

BalloonController::BalloonController() {
  pinMode(3, OUTPUT);
  digitalWrite(3, LOW);

  setState(STATE_INIT);

  Serial.begin(115200);
  printLog({ INFO, "[main]", "Initializing system..." });

  Wire.begin();


  if (bmp.begin(0x76) || bmp.begin(0x77)) {
    printLog({ INFO, "[bmp280]", "Initialized over I2C."});
  } else {
    printLog({ ERROR, "[bmp280]", "Error initializing. Check wiring."});
    setState(STATE_ERR);
    while(1) delay(10);
  }

  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X2,
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::FILTER_X16,
                  Adafruit_BMP280::STANDBY_MS_500);

  Serial1.setRxBufferSize(2048);
  Serial1.begin(115200, SERIAL_8N1, 7, 8, false);

  delay(3000);
  dht.begin();

  if (!isnan(dht.readHumidity()) && !isnan(dht.readTemperature())) {
    printLog({ INFO, "[dht22]", "DHT22 Connected and Sending Data." });
  } else {
    printLog({ ERROR, "[dht22]", "Error reading data. Check wiring." });
    printLog({ ERROR, "[dht22]", "Humidity: %s" }, char(h));
    printLog({ ERROR, "[dht22]", "Temperature: %f\n" }, h);
    currentState = STATE_ERR;
  }

  WiFi.mode(WIFI_STA);
  connectWiFi(networks);

  xTaskCreatePinnedToCore(
    _reconnectTask,
    "network",
    4096,
    nullptr,
    1,
    nullptr,
    0);

  printLog({ INFO, "[main]", "All systems initialized." });
}
