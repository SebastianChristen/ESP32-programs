#include <WebServer.h>
#include <Uri.h>
#include <HTTP_Method.h>

#include <WiFi.h>
#include "time.h"
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>

//0x3F or 0x27; twenty by four
LiquidCrystal_I2C lcd(0x27, 20, 4);  //LCD Object

// ---------- SENSITIVE STUFF, DON'T SHARE ----------------

// ---------- end of sensitive stuff ----------------


const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;
const char* wochenTage[] = { "So", "Mo", "Di", "Mi", "Do", "Fr", "Sa" };
const char* monate[] = { "Jan", "Feb", "März", "April", "Mai", "Juni", "Juli", "Aug", "Sept", "Okt", "Nov", "Dez" };

char weatherUrl[256];                              // Stellen Sie sicher, dass das Array groß genug ist, um die resultierende URL zu speichern
unsigned long previousMillis = 0;                  // Speichert den letzten Zeitstempel
const long interval = 120000;                      // Intervall von 2 Minuten (120000 Millisekunden)
char weatherStr[50] = "Warte auf Wetterdaten...";  // Initiale Wetteranzeige
char lastCharacters[256] = "Warte auf Daten...";   // Initialwert
String currentSong = "";



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
  pinMode(19, OUTPUT);  // Pin 19 als Ausgang definieren
}



void printLocalTime() {

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return;
  }

  char timeStr[10];
  char dateStr[30];
  // :%S für Sekunden
  strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
  //  snprintf(dateStr, sizeof(dateStr), "%s, %02d. %s %d",
  snprintf(dateStr, sizeof(dateStr), "%s, %02d. %s",
           wochenTage[timeinfo.tm_wday],
           timeinfo.tm_mday,
           monate[timeinfo.tm_mon],
           timeinfo.tm_year + 1900);
  lcd.setCursor(0, 0);
  lcd.print(timeStr);
  lcd.setCursor(7, 0);
  lcd.print(dateStr);

  if (timeinfo.tm_min == 0 && timeinfo.tm_sec == 0) {
    BlinkLicht();
  }
}


void printWeather() {

  //digitalWrite(19, HIGH);  // Pin 19 auf HIGH setzen

  HTTPClient http;
  http.begin(weatherUrl);
  http.addHeader("Authorization", "token " + String(authToken));


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
    float temperature = doc["main"]["temp"];

    snprintf(weatherStr, sizeof(weatherStr), "%.1fC %s", temperature, weatherDescription.c_str());
    lcd.setCursor(0, 1);
    Serial.println(weatherStr);
    lcd.print(weatherStr);
  } else {
    Serial.printf("HTTP GET request failed, code: %d\n", httpCode);
  }
  http.end();
  // digitalWrite(19, LOW);  // Pin 19 auf LOW setze
}


void printCurrentSong() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(user_url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      currentSong = extractNowPlaying(payload);  // Store the extracted song and artist

      // Print current song to the Serial Monitor
      Serial.println("Now Playing: " + currentSong);



      scrollText(currentSong);


    } else {
      Serial.println("Error in HTTP request");
    }

    http.end();
  } else {
    Serial.println("WiFi connection lost");
  }
}

void scrollText(String inputString) {
  inputString = inputString + "    ";  // Add spaces for smooth scrolling
  const int firstSegmentLength = 20;   // Fixed length for the first segment

  // Split the inputString into two parts: the first 20 characters and the rest
  String firstSegment = inputString.substring(0, firstSegmentLength);
  String secondSegment = inputString.substring(firstSegmentLength);  // The rest of the string after the first 20 characters

  // Loop indefinitely to scroll text
  for (int i = 0; i < inputString.length() + 1; i++) {
    // Print the first segment (fixed 20 characters)
    lcd.setCursor(0, 0);      // Set cursor to the start of the first line
    lcd.print(firstSegment);  // Print the first segment

    // Shift the first character of the first segment into the second segment
    char firstChar = firstSegment.charAt(0);   // Get the first character of the first segment
    firstSegment = firstSegment.substring(1);  // Remove the first character from the first segment
    secondSegment += firstChar;                // Append the removed character to the end of the second segment

    // Now take the first character from the second segment and prepend it to the first segment
    firstSegment += secondSegment.charAt(0);     // Add the first character of the second segment to the first segment
    secondSegment = secondSegment.substring(1);  // Remove the first character from the second segment

    // Ensure the first segment remains 20 characters long
    if (firstSegment.length() > firstSegmentLength) {
      firstSegment = firstSegment.substring(0, firstSegmentLength);  // Trim it to exactly 20 characters
    }

    // Delay to control the scroll speed
    delay(200);  // Adjust this delay for faster or slower scrolling
  }
}







String extractNowPlaying(String html) {
  String searchTag = "<strong>Now playing: </strong>";
  int startPos = html.indexOf(searchTag);

  if (startPos != -1) {
    // Suche nach dem Track-Namen (erster <a> Tag)
    startPos = html.indexOf("<a", startPos);          // Suche nach erstem <a>-Tag
    startPos = html.indexOf(">", startPos) + 1;       // Finde das Ende des <a>-Tags
    int endPos = html.indexOf("</a>", startPos);      // Finde das Ende des Link-Textes
    String track = html.substring(startPos, endPos);  // Extrahiere den Track-Namen
    track.trim();                                     // Bereinige den Track-Namen

    // Suche nach dem Künstler (zweiter <a> Tag, nach "by")
    startPos = html.indexOf("by", endPos);             // Suche nach "by"
    startPos = html.indexOf("<a", startPos);           // Suche nach dem nächsten <a>-Tag
    startPos = html.indexOf(">", startPos) + 1;        // Finde das Ende des zweiten <a>-Tags
    endPos = html.indexOf("</a>", startPos);           // Finde das Ende des zweiten Link-Textes
    String artist = html.substring(startPos, endPos);  // Extrahiere den Künstlernamen
    artist.trim();                                     // Bereinige den Künstlernamen

    // Rückgabe des Track-Namens und des Künstlers im Format "Song - Artist"
    return track + " - " + artist;
  }

  return "Kein 'Now Playing' gefunden.";
}







void fetchLastLine() {
  //digitalWrite(19, HIGH);  // Pin 19 auf HIGH setzen
  HTTPClient http;
  http.begin(fileUrl);
  // Setze den Authorization-Header mit dem Token
  http.addHeader("Authorization", "token " + String(authToken));


  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("Daten empfangen:");

    // Überprüfe, ob die Daten lang genug sind
    if (payload.length() > 20) {
      // Extrahiere die letzten 20 Zeichen
      String lastChars = payload.substring(payload.length() - 20);
      lastChars.toCharArray(lastCharacters, sizeof(lastCharacters));
    } else {
      // Wenn der Inhalt kürzer als 20 Zeichen ist
      payload.toCharArray(lastCharacters, sizeof(lastCharacters));
    }
    lcd.clear();
    Serial.println("Letzte Zeile:");
    Serial.println(lastCharacters);
    lcd.setCursor(0, 3);
    lcd.print(lastCharacters);



  } else {
    Serial.printf("HTTP GET request failed, code: %d\n", httpCode);
  }

  http.end();

  // digitalWrite(19, LOW);  // Pin 19 auf LOW setzen
}


void BlinkLicht() {
  int i = 0;
  while (i < 10) {
    digitalWrite(19, HIGH);  // Pin 19 auf HIGH setzen
    delay(500);
    digitalWrite(19, LOW);  // Pin 19 auf LOW setzen
    delay(500);
    i++;
  }
}

void sendLight() {
  digitalWrite(19, HIGH);  // Pin 19 auf HIGH setzen
  delay(500);
  digitalWrite(19, LOW);  // Pin 19 auf LOW setzen
}




void loop() {
  //printLocalTime();  // Zeige die lokale Zeit an
  //BlinkLicht();
  printCurrentSong();

  unsigned long currentMillis = millis();


  // Überprüfe, ob das Intervall von 2 Minuten verstrichen ist
  // if (currentMillis - previousMillis >= interval) {
  //   previousMillis = currentMillis;  // Aktuellen Zeitstempel speichern
  //   printWeather();                  // Überprüfe das Wetter
  //fetchLastLine();                 // Fetch the last line
  // }

  delay(1000);  // Warte 1 Sekunde, bevor die Schleife erneut durchläuft
}
