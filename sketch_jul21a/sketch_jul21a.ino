#include <WebServer.h>
#include <Uri.h>
#include <HTTP_Method.h>

#include <WiFi.h>
#include "time.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <LiquidCrystal_I2C.h>


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
    scrollText(weatherStr);
  } else {
    Serial.printf("HTTP GET request failed, code: %d\n", httpCode);
  }
  http.end();
}


void printCurrentSong() {
  lcd.setCursor(0, 3);
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(user_url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      currentSong = extractNowPlaying(payload);  // Store the extracted song and artist

      Serial.println("Vor der Transliterierung: " + currentSong);

      String currentSongDeutsch = transliterateRussianToGerman(currentSong);

      Serial.println("Nach der Transliterierung: " + currentSongDeutsch);

      scrollText(currentSongDeutsch);

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
    lcd.setCursor(0, 3);      // Set cursor to the start of the first line
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
    delay(250);  // Adjust this delay for faster or slower scrolling
  }
}

String transliterateRussianToGerman(String russianText) {
  String germanTranslit = "";
  uint16_t currentChar;

  for (int i = 0; i < russianText.length(); i++) {
    // Lies das aktuelle Zeichen als unsigned char (UTF-8 kompatibel)
    currentChar = (unsigned char)russianText[i];

    // Prüfe ob das aktuelle Zeichen ein Mehr-Byte UTF-8 Zeichen ist
    if (currentChar >= 0xC0) {
      // UTF-8 kodierte Zeichen mit zwei Bytes lesen
      currentChar = ((currentChar & 0x1F) << 6) | (russianText[i + 1] & 0x3F);
      i++;  // Springe ein Zeichen weiter, da wir 2 Bytes gelesen haben
    }

    // Transliteration für kyrillische Buchstaben
    switch (currentChar) {
      case 0x0410: germanTranslit += "A"; break; // А
      case 0x0430: germanTranslit += "a"; break; // а
      case 0x0411: germanTranslit += "B"; break; // Б
      case 0x0431: germanTranslit += "b"; break; // б
      case 0x0412: germanTranslit += "W"; break; // В
      case 0x0432: germanTranslit += "w"; break; // в
      case 0x0413: germanTranslit += "G"; break; // Г
      case 0x0433: germanTranslit += "g"; break; // г
      case 0x0414: germanTranslit += "D"; break; // Д
      case 0x0434: germanTranslit += "d"; break; // д
      case 0x0415: germanTranslit += "Je"; break; // Е
      case 0x0435: germanTranslit += "je"; break; // е
      case 0x0401: germanTranslit += "Jo"; break; // Ё
      case 0x0451: germanTranslit += "jo"; break; // ё
      case 0x0416: germanTranslit += "Sch"; break; // Ж
      case 0x0436: germanTranslit += "sch"; break; // ж
      case 0x0417: germanTranslit += "S"; break; // З
      case 0x0437: germanTranslit += "s"; break; // з
      case 0x0418: germanTranslit += "I"; break; // И
      case 0x0438: germanTranslit += "i"; break; // и
      case 0x0419: germanTranslit += "J"; break; // Й
      case 0x0439: germanTranslit += "j"; break; // й
      case 0x041A: germanTranslit += "K"; break; // К
      case 0x043A: germanTranslit += "k"; break; // к
      case 0x041B: germanTranslit += "L"; break; // Л
      case 0x043B: germanTranslit += "l"; break; // л
      case 0x041C: germanTranslit += "M"; break; // М
      case 0x043C: germanTranslit += "m"; break; // м
      case 0x041D: germanTranslit += "N"; break; // Н
      case 0x043D: germanTranslit += "n"; break; // н
      case 0x041E: germanTranslit += "O"; break; // О
      case 0x043E: germanTranslit += "o"; break; // о
      case 0x041F: germanTranslit += "P"; break; // П
      case 0x043F: germanTranslit += "p"; break; // п
      case 0x0420: germanTranslit += "R"; break; // Р
      case 0x0440: germanTranslit += "r"; break; // р
      case 0x0421: germanTranslit += "S"; break; // С
      case 0x0441: germanTranslit += "s"; break; // с
      case 0x0422: germanTranslit += "T"; break; // Т
      case 0x0442: germanTranslit += "t"; break; // т
      case 0x0423: germanTranslit += "U"; break; // У
      case 0x0443: germanTranslit += "u"; break; // у
      case 0x0424: germanTranslit += "F"; break; // Ф
      case 0x0444: germanTranslit += "f"; break; // ф
      case 0x0425: germanTranslit += "Ch"; break; // Х
      case 0x0445: germanTranslit += "ch"; break; // х
      case 0x0426: germanTranslit += "Z"; break; // Ц
      case 0x0446: germanTranslit += "z"; break; // ц
      case 0x0427: germanTranslit += "Tsch"; break; // Ч
      case 0x0447: germanTranslit += "tsch"; break; // ч
      case 0x0428: germanTranslit += "Sch"; break; // Ш
      case 0x0448: germanTranslit += "sch"; break; // ш
      case 0x0429: germanTranslit += "Schtsch"; break; // Щ
      case 0x0449: germanTranslit += "schtsch"; break; // щ
      case 0x042B: germanTranslit += "Y"; break; // Ы
      case 0x044B: germanTranslit += "y"; break; // ы
      case 0x042D: germanTranslit += "E"; break; // Э
      case 0x044D: germanTranslit += "e"; break; // э
      case 0x042E: germanTranslit += "Ju"; break; // Ю
      case 0x044E: germanTranslit += "ju"; break; // ю
      case 0x042F: germanTranslit += "Ja"; break; // Я
      case 0x044F: germanTranslit += "ja"; break; // я


      //case 0x042A: // Ъ (Hard-Zeichen, Großbuchstabe)
      //case 0x044A: // ъ (Hard-Zeichen, Kleinbuchstabe)
      // Keine Transliteration oder falls du möchtest: germanTranslit += "\""; break;

       case 0x042C: germanTranslit += "'"; break;// Ь (Soft-Zeichen, Großbuchstabe)
       case 0x044C: germanTranslit += "'"; break;// ь (Soft-Zeichen, Kleinbuchstab
      default: germanTranslit += (char)currentChar; // Nicht-russische Zeichen hinzufügen
    }
  }

  return germanTranslit;
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
  printLocalTime();  // Zeige die lokale Zeit an
  printCurrentSong();


  unsigned long currentMillis = millis();

  // Überprüfe, ob das Intervall von 2 Minuten verstrichen ist
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;  // Aktuellen Zeitstempel speichern
    printWeather();                  // Überprüfe das Wetter
    //fetchLastLine();               // Fetch the last line
  }

  //delay(500);  // Warte 1 Sekunde, bevor die Schleife erneut durchläuft
}
