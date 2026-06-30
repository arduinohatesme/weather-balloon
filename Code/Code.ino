#include "Arduino.h"
#include "src/BalloonController.h"

BalloonController balloon{};

const int interval = 1000;
float gust = -1.0;
int lastTime = 0;
const NetworkInfo networks[2] = {
    {"Launchpad Internal", "L@unchP@d!nc"},
    {"Bill Clinternet", "UsmC2336"},
};

/*
 * APRS Formatting -------------------------
 * Name               - B | e.g.    - Notes
 * Start symbol       - 1 | /
 * Time (HHMMSSz)     - 7 | 083020z
 * Latitude           - 8 | 4903.50N
 * Separator symbol   - 1 | /
 * Longitude          - 9 | 07201.75W
 * Symbol code        - 1 | _
 * Wind directn/speed - 7 | 220/004 - Maybe?
 * Weather data ----------------------------
 * Wind directn       - 4 | c220    - Maybe?
 * Wind speed         - 4 | s004    - Maybe?
 * Wind gust          - 4 | g006
 * Temperature        - 4 | 072     - Fahrenheit
 * Humidity           - 3 | h20     - %
 * Pressure           - 5 | b1013   - 1/10ths of mbars
 * -----------------------------------------
 */

void setup() {}

// TODO: clear gust after 15m
void loop() {
  balloon.streamToGPS(Serial1);

  if (millis() - lastTime <= interval)
    return;

  lastTime = millis();
  SensorReadings data = balloon.readSensors();

  char *buf = "";
  Serial.println(balloon.formatReadings(data, buf));

  gust = max(gust, data.wind_speed);

  if (balloon.getState() != STATE_OK) {
    balloon.printLog(
        {LOG_ERROR, "[main]", "Something went wrong. Check logs above."});
  };
}
