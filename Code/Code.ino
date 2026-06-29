#include <Wire.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <WiFi.h>
#include "time.h"
#include <TinyGPSPlus.h>
#include <math.h>

#define DHT22_PIN 2
#define DHTTYPE DHT22
#define ERR_VAL -999.0

#define RGB_BRIGHTNESS 64
#if defined(RGB_BUILTIN)
#define LED_PIN RGB_BUILTIN
#else
#define LED_PIN 48
#endif

enum SystemState {
  BOOTING,
  SYSTEM_OK,
  READ_ERROR,
  HARDWARE_FIX
};

enum LogSeverity {
  DEBUG,
  INFO,
  WARNING,
  ERROR,
};

const char* sevStrs[] = { "DEBUG", "INFO", "WARNING", "ERROR" };

struct NetworkInfo {
  char* ssid;
  char* pass;
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
  char* task;
  char* message;
};

const char* ntpServer = "pool.ntp.org";

float last_v = 0;
unsigned long last_time = 0;

DHT dht(DHT22_PIN, DHTTYPE);
Adafruit_BMP280 bmp;
TinyGPSPlus gps;
SystemState currentState = BOOTING;

void printLog(LogMessage l, ...) {
  char buf[128];

  va_list args;
  va_start(args, l);
  vsnprintf(buf, sizeof(buf), l.message, args);
  va_end(args);

  const char* sevStr = sevStrs[(int)l.severity];

  Serial.printf("[%s] [%s] %s\n", sevStr, l.task, buf);
}

void setBuiltInLED(SystemState state) {
  switch (state) {
    case BOOTING:
      neopixelWrite(LED_PIN, 0, 0, RGB_BRIGHTNESS);
      break;
    case SYSTEM_OK:
      neopixelWrite(LED_PIN, 0, RGB_BRIGHTNESS, 0);
      break;
    case READ_ERROR:
      neopixelWrite(LED_PIN, RGB_BRIGHTNESS, RGB_BRIGHTNESS / 2, 0);
      break;
    case HARDWARE_FIX:
      neopixelWrite(LED_PIN, RGB_BRIGHTNESS, 0, 0);
      break;
  }
}

SensorReadings readSensors() {
  float dhtTemp = dht.readTemperature();
  float bmpTemp = bmp.readTemperature();
  float humidity = dht.readHumidity();
  float pressure = bmp.readPressure();

  if (isnan(dhtTemp) || isnan(bmpTemp) || isnan(humidity) || isnan(pressure)) {
    currentState = READ_ERROR;
    SensorReadings errResult = { ERR_VAL, ERR_VAL, ERR_VAL, ERR_VAL, ERR_VAL, ERR_VAL, ERR_VAL, ERR_VAL };
    return errResult;
  }

  float hectoPascals = (pressure / 100.0);

  SensorReadings result;
  result.DHT_temp = dhtTemp;
  result.BMP_temp = bmpTemp;
  result.humidity = humidity;
  result.pressure = hectoPascals;

  if (gps.location.isValid()) {
    result.latitude = gps.location.lat();
    result.longitude = gps.location.lng();
    result.wind_speed = gps.speed.mps();
    result.wind_dir = fmod(gps.course.deg() + 180, 360);
    currentState = SYSTEM_OK;
  } else {
    result.latitude = ERR_VAL;
    result.longitude = ERR_VAL;
    result.wind_speed = 0;
    result.wind_dir = 0;
  }

  return result;
}

void DHT_Startup() {
  delay(3000);
  dht.begin();
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    printLog({ ERROR, "dht22", "Error reading data. Check wiring." });
    printLog({ ERROR, "[dht22]", "Humidity: %s" }, char(h));
    printLog({ ERROR, "[dht22]", "Temperature: %f\n" }, h);
    currentState = READ_ERROR;
    return;
  }
  printLog({ INFO, "[dht22]", "DHT22 Connected and Sending Data." });
}

bool timeSynced = false;

const NetworkInfo networks[2] = {
  { "Launchpad Internal", "L@unchP@d!nc" },
  { "Bill Clinternet", "UsmC2336" },
};

void connectWithTimeout(const NetworkInfo* nets) {
  for (int i = 0; i < 2; i++) {
    printLog({ DEBUG, "[network]"
                      "Trying %s" },
             nets[i].ssid);
    WiFi.begin(nets[i].pass, nets[i].ssid);

    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 20000) {
      delay(500);
      Serial.print(".");
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      printLog({ INFO, "[network]"
                       "Connected to %s\n" },
               WiFi.SSID().c_str());
      configTime(0, 0, ntpServer);
      return;
    }

    printLog({ WARNING, "[network]"
                        "Connection attempt failed to network: %s\n" },
             nets[i].ssid);
  }
  printLog({ ERROR, "[network]"
                    "All connection attempts failed." });
}

void WiFiReconnectorTask(void* pvParameters) {
  while (WiFi.status() != WL_CONNECTED) {
    printLog({ WARNING, "[network]"
                        "Connection lost. Retrying..." });
    connectWithTimeout(networks);
    vTaskDelay(30000 / portTICK_PERIOD_MS);
  }
}

void setup() {

  pinMode(3, OUTPUT);
  digitalWrite(3, LOW);

  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && millis() - start < 3000);

  setBuiltInLED(BOOTING);
  printLog({ INFO, "[init]", "Initializing system..." });

  Wire.begin();

  if (!bmp.begin(0x76) && !bmp.begin(0x77)) {
    printLog({ ERROR, "[bmp280]", "Error connecting over I2C. Check wiring." });
    currentState = HARDWARE_FIX;
    setBuiltInLED(HARDWARE_FIX);
    while (1) delay(10);
  }

  Serial1.setRxBufferSize(2048);
  Serial1.begin(115200, SERIAL_8N1, 7, 8, false);

  DHT_Startup();

  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X2,
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::FILTER_X16,
                  Adafruit_BMP280::STANDBY_MS_500);

  WiFi.mode(WIFI_STA);
  connectWithTimeout(networks);

  printLog({ INFO, "[init]", "All systems initialized." });

  xTaskCreatePinnedToCore(
    WiFiReconnectorTask,
    "WiFiTask",
    4096,
    NULL,
    1,
    NULL,
    0);
}

String getCoordinates(SensorReadings data) {
  if (!gps.location.isValid()) return "0000.00N/00000.00W_";

  int latDeg = (int)data.latitude;
  double latMin = (data.latitude - latDeg) * 60.0;
  char latStr[9];
  sprintf(latStr, "%02d%05.2f%c", abs(latDeg), latMin, (latDeg >= 0) ? 'N' : 'S');

  int lngDeg = (int)data.longitude;
  double lngMin = (data.longitude - lngDeg) * 60.0;
  char lngStr[10];
  sprintf(lngStr, "%03d%05.2f%c", abs(lngDeg), lngMin, (lngDeg >= 0) ? 'E' : 'W');

  return latStr, "/", lngStr, "_";
}

float WindGust = 0;

String internalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    printLog({ WARNING, "[time]", "Time not set yet. Sync with NTP first." });
    return "000000";
  }
  char buf[16];
  snprintf(buf, sizeof(buf), "%02d%02d%02d", (const char*)timeinfo.tm_mday, (const char*)timeinfo.tm_hour, (const char*)timeinfo.tm_min);
  return String(buf);
}

String formatTemp(float tempF) {
  int tempRounded = (int)round(tempF);
  char buffer[5];
  sprintf(buffer, "%03d", tempRounded % 1000);
  return String(buffer);
}

String formatPressure(float pressure) {
  char buffer[7];
  sprintf(buffer, "%05d", (int)((long)pressure % (long)100000.0));
  return String(buffer);
}

String formatHumidity(float humidity) {
  int humidityRounded = (int)round(humidity);
  char buffer[4];
  sprintf(buffer, "%02d", humidityRounded % 100);
  return String(buffer);
}

void printMeasurements(SensorReadings data) {
  Serial.printf(
    "%s\n---------------------------@%s/t%sh%sb%s\n",
    getCoordinates(data).c_str(),
    internalTime().c_str(),
    formatTemp(data.BMP_temp).c_str(),
    formatHumidity(data.humidity).c_str(),
    formatPressure(data.pressure).c_str());
}

unsigned long lastTime = 0;
const unsigned long interval = 1000;

bool currentInversion = false;
long currentBaud = 115200;

void loop() {
  int bytesProcessed = 0;
  for (int i = 0; i < 128 && Serial1.available() > 0; i++) {
    char c = Serial1.read();

    if ((c >= 32 && c <= 126) || c == '\r' || c == '\n') {
      Serial.write(c);
      gps.encode(c);
    }
  }

  if (millis() - lastTime <= interval) return;

  lastTime = millis();
  SensorReadings data = readSensors();
  setBuiltInLED(currentState);

  printMeasurements(data);

  WindGust = max(WindGust, data.wind_speed);

  if (currentState != SYSTEM_OK) {
    printLog({ ERROR, "[main]", "Something went wrong. Check logs above." });
    currentState = READ_ERROR;
  };

  printLog({ INFO, "[gps]", "Looking for satellites. Found: %s" }, (char*)gps.satellites.value());
  currentState = READ_ERROR;
}
