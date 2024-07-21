#include <WebServer.h>
#include <Uri.h>
#include <HTTP_Method.h>

#include <WiFi.h>
#include "time.h"
#include <HTTPClient.h>
#include <LCDI2C_Multilingual.h>
#include <ArduinoJson.h>

//0x3F or 0x27; twenty by four

LCDI2C_Symbols lcd(0x27, 20, 4);  //LCD Object

// ---------- SENSITIVE STUFF, DON'T SHARE ----------------
const char* ssid = "";
const char* password = "";
const char* apiKey = "";
const char* city = "";
const char* country = "";
// ---------- end of sensitive stuff ----------------


const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;
const char* wochenTage[] = { "So", "Mo", "Di", "Mi", "Do", "Fr", "Sa" };
const char* monate[] = { "Januar", "Februar", "März", "April", "Mai", "Juni", "Juli", "August", "September", "Oktober", "November", "Dezember" };

bool smile = false;


char weatherUrl[256];  // Stellen Sie sicher, dass das Array groß genug ist, um die resultierende URL zu speichern

const unsigned long TWO_MINUTES = 120000;  // Zwei Minuten in Millisekunden
unsigned long lastWeatherCheck = 110000;
char weatherStr[50] = "Warte auf Wetterdaten...";  // Initiale Wetteranzeige


void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.clear();
  lcd.backlight();
  lcd.print("starten...");

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }

  // Initialize and configure time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
  sprintf(weatherUrl, "http://api.openweathermap.org/data/2.5/weather?q=%s,%s&appid=%s&units=metric&lang=de", city, country, apiKey);


  lcd.clear();
}



void printLocalTime() {

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {

    return;
  }

  char timeStr[10];
  char dateStr[30];
  // :%S für Sekunden
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
  snprintf(dateStr, sizeof(dateStr), "%s, %02d. %s %d",
           wochenTage[timeinfo.tm_wday],
           timeinfo.tm_mday,
           monate[timeinfo.tm_mon],
           timeinfo.tm_year + 1900);


  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(timeStr);
  lcd.setCursor(0, 1);
  lcd.print(dateStr);
  lcd.setCursor(0, 2);

  smile = !smile;

  if (smile) {
    //lcd.print("-ш-");
  } else {
    //lcd.print("^ш^");
  }
}


void printWeather() {
  unsigned long currentMillis = millis();  // Hole die aktuelle Zeit in Millisekunden

  if (currentMillis - lastWeatherCheck >= TWO_MINUTES) {
    lastWeatherCheck = currentMillis;  // Aktualisiere den Zeitstempel des letzten Wetter-Checks

    HTTPClient http;
    http.begin(weatherUrl);

    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println(payload);  // Print JSON response for debugging

      StaticJsonDocument<2048> doc;  // Increase the buffer size for JSON parsing
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.f_str());
        return;
      }
      String weatherDescription = doc["weather"][0]["description"].as<String>();
      weatherDescription.toLowerCase();  // Convert description to lowercase
      float temperature = doc["main"]["temp"];

      snprintf(weatherStr, sizeof(weatherStr), "Temp: %.1fC %s", temperature, weatherDescription.c_str());
    } else {
      Serial.printf("HTTP GET request failed, code: %d\n", httpCode);
    }
    http.end();
  }
  lcd.setCursor(0, 2);
  Serial.println(weatherStr);
  lcd.print(weatherStr);
}



void loop() {
  printLocalTime();  // Zeige die lokale Zeit an
  printWeather();    // Überprüfe das Wetter
  delay(1000);       // Warte 1 Sekunde, bevor du die Schleife erneut durchläufst
}
