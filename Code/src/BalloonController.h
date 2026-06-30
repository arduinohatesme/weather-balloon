/*
 * BalloonController.h - Controller for Launchpad Incubator's weather balloon
 * project Created by Aidan McMillan, June 29, 2026. Released into the public
 * domain.
 */

#ifndef BalloonController_h

#define BalloonController_h

#include "Arduino.h"

#include "HardwareSerial.h"
#include "time.h"
#include <Adafruit_BMP280.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <TinyGPSPlus.h>
#include <cstdint>

enum SystemState {
  STATE_INIT,
  STATE_OK,
  STATE_ERR,
};

enum LogSeverity {
  LOG_DEBUG,
  LOG_INFO,
  LOG_WARN,
  LOG_ERROR,
};

const char *sevStrs[] = {"DEBUG", "INFO", "WARNING", "ERROR"};

struct NetworkInfo {
  const char *ssid;
  const char *pass;
};

struct SensorReadings {
  float DHT_temp;
  float BMP_temp;
  float humidity;
  float pressure;
  double latitude;
  double longitude;
  float wind_speed;
  float wind_dir;
};

struct LogMessage {
  LogSeverity severity;
  const char *task;
  const char *message;
};

class BalloonController {
public:
  BalloonController();
  SensorReadings readSensors();
  void formatReadings(SensorReadings readings, char *dest);
  void printLog(LogMessage l, ...);
  void streamToGPS(HardwareSerial uartin);

  SystemState getState();
  void setState(SystemState newState);
  void connectWiFi(const NetworkInfo nets[], int to_ms = 10);

private:
  String _formatHumidity(float tempF);
  String _formatPressure(float pa);
  String _formatTemp(float rhp);
  String _formatCoords(float latDeg, float lngDeg);
  String _formatTime();

  void _reconnectTask();
  static void _reconnectTaskWrapped();

  const char *NTP_SERVER = "pool.ntp.org";
  const int LED_PIN = 48;
  const uint8_t DHT_PIN = 2;
  const int LED_BRIGHTNESS = 64;
  const float ERR_VAL = 999.0;
  const uint8_t DHTTYPE = 22;

  DHT dht{DHT_PIN, DHTTYPE};
  Adafruit_BMP280 bmp;
  TinyGPSPlus gps;
  SystemState currentState = STATE_INIT;

  SystemState _state;
};

#endif
