#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const char *ssid = "wifi_ssid";
const char *password = "wifi_password";
const char *property_id = "123"; // found in the URL, https://app.codefund.io/properties/123
const char cookie[] PROGMEM = "Cookie: remember_user_token=eyJf.......\r\n";
// TO FIND THIS COOKIE:
// Go to app.codefund.io
// F12 -> Storage -> Cookies -> app.codefund.io
// Replace remember_user_token value


const char appFingerprint[] PROGMEM = "41 FF FA 14 78 D6 E9 8C 71 65 98 7E 17 27 B3 CE 29 07 27 72";
const char metabaseFingerprint[] PROGMEM = "99 3D AA 94 9A FA BA 71 46 1A 8C F8 26 2F 2D C2 D0 0F 36 41";
const char worldtimeapiFingerprint[] PROGMEM = "49 13 DA AE 65 7B B5 20 82 F6 71 CD A9 CB 45 9C D2 89 47 FF";
const char useragent[] PROGMEM = "User-Agent: NodeMCU (github.com/sh4dowb/codefund-esp-monitor)\r\n";

Adafruit_SSD1306 display;
int fetched = 0;
String daterange = "";
String token = "";

void setup() {
  Wire.begin(4, 0);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("wi-fi connecting");
  display.display();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  display.clearDisplay();
  printFirstLine("date updating");
  int fail = 0;
  while (!updateDatetime()) {
    fail++;
    printFirstLine("date updating (" + String(fail) + ")");
    checkWifi();
    delay(1000);
  }
}


void printFirstLine(String text) {
  display.fillRect(0, 0, display.width(), 8, BLACK);
  display.setCursor(0, 0);
  display.println(text);
  display.display();
}

void checkWifi() {
  if (WiFi.status() != WL_CONNECTED) {
    for (int i = 10; i > 0; i--) {
      printFirstLine(String("wi-fi lost, rst in ") + String(i));
      if (i == 0)
        ESP.reset();
      delay(1000);
    }
  }
}

boolean updateDatetime() {
  //maybe implement NTP instead?
  WiFiClientSecure httpsClient;

  httpsClient.setFingerprint(worldtimeapiFingerprint);
  httpsClient.setTimeout(15000);
  int r = 0;
  while ((!httpsClient.connect("worldtimeapi.org", 443)) && (r < 30)) {
    delay(100);
    r++;
  }
  if (r == 30)
    return false;

  httpsClient.print(String("GET /api/timezone/Europe/London.txt HTTP/1.1\r\n") +
                    "Host: worldtimeapi.org\r\n" +
                    useragent +
                    cookie +
                    "\r\n");


  while (httpsClient.connected()) {
    String line = httpsClient.readStringUntil('\n');
    if (line.substring(0, 14) == "utc_datetime: ") {
      String ymd = line.substring(14, 14 + 10);
      String day = ymd.substring(8, 8 + 2);
      String month = ymd.substring(5, 5 + 2);
      String year = ymd.substring(0, 0 + 4);
      String olddaterange = daterange;
      daterange = month + "%2F" + "01" + "%2F" + year + "+-+" + month + "%2F" + day + "%2F" + year;
      if (daterange != olddaterange) {
	// token changes with the date. if date changed, update the token.
        printFirstLine("token updating");
        int fail = 0;
        while (!updateToken()) {
          fail++;
          printFirstLine("token updating (" + String(fail) + ")");
          checkWifi();
          delay(1000);
        }
      }
      return true;
      break;
    }
  }
  return false;
}

boolean updateToken() {
  //WiFiClientSecure httpsClient; // SSL was working before. then it stopped working. I have no idea why, lol.
  WiFiClient httpsClient;

  //httpsClient.setFingerprint(appFingerprint);
  httpsClient.setTimeout(15000);
  int r = 0;
  while ((!httpsClient.connect("app.codefund.io", 80)) && (r < 30)) {
    delay(100);
    r++;
  }
  if (r == 30)
    return false;

  httpsClient.print(String("GET /properties/") + property_id + "?date_range=" + daterange + " HTTP/1.1\r\n" +
                    "Host: app.codefund.io\r\n" +
                    useragent +
                    cookie +
                    "\r\n");


  while (httpsClient.connected()) {
    String line = httpsClient.readStringUntil('\n');
    if (line == "<!DOCTYPE html>") // \r should work, but doesn't for some reason. this works fine too.
      break;
  }
  while (httpsClient.available()) {
    String line = httpsClient.readStringUntil('\n');
    if (line == "  <iframe") {
      token = httpsClient.readStringUntil('#').substring(54);
      if (token != "")
        return true;
    }
  }
  return false;
}


void loop() {
  checkWifi();
  printFirstLine("balance updating");
  WiFiClientSecure httpsClient;

  httpsClient.setFingerprint(metabaseFingerprint);
  httpsClient.setTimeout(15000);
  int r = 0;
  while ((!httpsClient.connect("metabase.codefund.io", 443)) && (r < 30)) {
    delay(100);
    r++;
  }
  if (r == 30) {
    printFirstLine("metabase conn fail");
    delay(10000);
    return;
  }

  httpsClient.print(String("GET /api/embed/dashboard/") + token + "/dashcard/230/card/274" + " HTTP/1.1\r\n" +
                    "Host: metabase.codefund.io\r\n" +
                    useragent +
                    cookie +
                    "\r\n");


  while (httpsClient.connected()) {
    String line = httpsClient.readStringUntil('\n');
    if (line == "\r")
      break;
  }


  boolean startRevenue = false;
  float month = 0;
  float today = 0;
  boolean dataread = false;

  // parse daily revenues
  while (httpsClient.available()) {
    dataread = true;
    if (!startRevenue) {
      String line = httpsClient.readStringUntil(',');
      if (line.substring(0, 8) == "\"rows\":[")
        startRevenue = true;
    } else {
      String data = httpsClient.readStringUntil(']');
      today = data.toFloat();
      month += today;
      if (httpsClient.readStringUntil(',') == "]")
        break;
      httpsClient.readStringUntil(',');
    }
  }

  // sometimes no data is returned
  if (!dataread) {
    printFirstLine("metabase parse fail");
    delay(10000);
    return;
  }
  display.clearDisplay();

  display.setTextSize(2);
  display.setCursor(0, 8);
  display.println(String("$") + String(today));

  display.setTextSize(1);
  display.setCursor(0, 23);
  display.println(String("Total: $") + String(month));

  display.display();


  // every 10 minutes update the date
  fetched++;
  if (fetched == 5) {
    printFirstLine("date updating");
    int fail = 0;
    while (!updateDatetime()) {
      fail++;
      printFirstLine("date updating (" + String(fail) + ")");
      checkWifi();
      delay(1000);
    }
    fetched = 0;
  }

  for (int i = 120; i > 0; i--) {
    printFirstLine("update in " + String(i));
    delay(1000);
  }
}
