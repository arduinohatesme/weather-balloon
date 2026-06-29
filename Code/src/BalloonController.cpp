/*
 * BalloonController.cpp - Controller for Launchpad Incubator's weather balloon
 * project Created by Aidan McMillan, June 29, 2026. Released into the public
 * domain.
 */

#include "BalloonController.h"
#include "Arduino.h"

#include <WiFi.h>
#include <Wire.h>
#include <math.h>

BalloonController::BalloonController() {
  pinMode(3, OUTPUT);
  digitalWrite(3, LOW);

  setState(STATE_INIT);

  Serial.begin(115200);
  printLog({LOG_INFO, "[main]", "Initializing system..."});

  Wire.begin();

  if (bmp.begin(0x76) || bmp.begin(0x77)) {
    printLog({LOG_INFO, "[bmp280]", "Initialized over I2C."});
  } else {
    printLog({LOG_ERROR, "[bmp280]", "Error initializing. Check wiring."});
    setState(STATE_ERR);
    while (1)
      delay(10);
  }

  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL, Adafruit_BMP280::SAMPLING_X2,
                  Adafruit_BMP280::SAMPLING_X16, Adafruit_BMP280::FILTER_X16,
                  Adafruit_BMP280::STANDBY_MS_500);

  Serial1.setRxBufferSize(2048);
  Serial1.begin(115200, SERIAL_8N1, 7, 8, false);

  delay(3000);
  dht.begin();

  if (!isnan(dht.readHumidity()) && !isnan(dht.readTemperature())) {
    printLog({LOG_INFO, "[dht22]", "DHT22 Connected and Sending Data."});
  } else {
    printLog({LOG_ERROR, "[dht22]", "Error reading data. Check wiring."});
    printLog({LOG_ERROR, "[dht22]", "Humidity: %s"}, char(h));
    printLog({LOG_ERROR, "[dht22]", "Temperature: %f\n"}, h);
    currentState = STATE_ERR;
  }

  WiFi.mode(WIFI_STA);
  connectWiFi(networks);

  xTaskCreatePinnedToCore(_reconnectTask, "network", 4096, nullptr, 1, nullptr,
                          0);

  printLog({LOG_INFO, "[main]", "All systems initialized."});
}

String BalloonController::_formatTemp(float tempF) {
  int tempRounded = (int)round(tempF);
  char buffer[5];
  sprintf(buffer, "%03d", tempRounded % 1000);
  return String(buffer);
}

String BalloonController::_formatPressure(float pressure) {
  char buffer[7];
  sprintf(buffer, "%05d", (int)((long)pressure % (long)100000.0));
  return String(buffer);
}

String BalloonController::_formatHumidity(float humidity) {
  int humidityRounded = (int)round(humidity);
  char buffer[4];
  sprintf(buffer, "%02d", humidityRounded % 100);
  return String(buffer);
}

void BalloonController::_formatCoords(float latDeg, float lngDeg, char *dest) {
  if (!gps.location.isValid()) {
    strcpy(dest, "0000.00N/00000.00W_");
    return;
  }

  char latStr[9];
  sprintf(latStr, "%07.2f%c", latDeg, (latDeg >= 0) ? 'N' : 'S');

  char lngStr[10];
  sprintf(lngStr, "%08.2f%c", lngDeg, (lngDeg >= 0) ? 'E' : 'W');

  sprintf(dest, "%s/%s_", latStr, lngStr)
}

String BalloonController::formatReadings(SensorReadings data, char *dest) {
  sprintf(dest, "%s\n---------------------------@%s/t%sh%sb%s\n",
          getCoordinates(data), internalTime(), formatTemp(data.BMP_temp),
          formatHumidity(data.humidity), formatPressure(data.pressure), );
}
