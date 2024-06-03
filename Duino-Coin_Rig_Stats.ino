#include <Wire.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h> 

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ 14, /* data=*/ 12, /* reset=*/ U8X8_PIN_NONE);

const char *ssid = ""; // Change this to your WiFi SSID
const char *password = ""; // Change this to your WiFi password
const String ducoUser = ""; // Change this to your Duino-Coin username
const String ducoReportJsonUrl = "https://server.duinocoin.com/v2/users/" + ducoUser + "?limit=1";
const int run_in_ms = 2000;
float lastAverageHash = 0.0;
float lastTotalHash = 0.0;

void setup() {
    Serial.begin(115200);
    initDisplayOled();
    setupWifi();
}

void setupWifi() {
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    // Display "Setup WiFi" message on the OLED
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr); // Choose a suitable font
    u8g2.drawStr(0, 10, "Setup WiFi");
    u8g2.drawStr(0, 30, "Connecting...");
    u8g2.sendBuffer();

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    // Display WiFi connected message
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 10, "WiFi Connected!");
    u8g2.drawStr(0, 30, WiFi.localIP().toString().c_str());
    u8g2.sendBuffer();
    delay(2000); // Wait for 2 seconds before showing stats
}

String httpGetString(String URL) {
    String payload = "";
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    if (http.begin(client, URL)) {
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            payload = http.getString();
        } else {
            Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();
    }
    return payload;
}

void initDisplayOled() {
    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.sendBuffer();
}

boolean runEvery(unsigned long interval) {
    static unsigned long previousMillis = 0;
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        return true;
    }
    return false;
}

String formatHashrate(float hashrate) {
    String result;
    if (hashrate >= 1e18) {
        result = String(hashrate / 1e18, 2) + " EH/s";
    } else if (hashrate >= 1e15) {
        result = String(hashrate / 1e15, 2) + " PH/s";
    } else if (hashrate >= 1e12) {
        result = String(hashrate / 1e12, 2) + " TH/s";
    } else if (hashrate >= 1e9) {
        result = String(hashrate / 1e9, 2) + " GH/s";
    } else if (hashrate >= 1e6) {
        result = String(hashrate / 1e6, 2) + " MH/s";
    } else if (hashrate >= 1e3) {
        result = String(hashrate / 1e3, 2) + " kH/s";
    } else {
        result = String(hashrate, 2) + " H/s";
    }
    return result;
}

void loop() {
    if (runEvery(run_in_ms)) {

        float totalHashrate = 0.0;
        float avgHashrate = 0.0;
        int total_miner = 0;
        String input = httpGetString(ducoReportJsonUrl);
        DynamicJsonDocument doc(8000);
        DeserializationError error = deserializeJson(doc, input);
        if (error) {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
            return;
        }
        JsonObject result = doc["result"];
        JsonObject result_balance = result["balance"];
        double result_balance_balance = result_balance["balance"];
        const char *result_balance_created = result_balance["created"];
        const char *result_balance_username = result_balance["username"];
        const char *result_balance_verified = result_balance["verified"];

        for (JsonObject result_miner : result["miners"].as<JsonArray>()) {
            float result_miner_hashrate = result_miner["hashrate"];
            totalHashrate = totalHashrate + result_miner_hashrate;
            total_miner++;
        }

        avgHashrate = totalHashrate / long(total_miner);
        long run_span = run_in_ms / 1000;

        String formattedTotalHashrate = formatHashrate(totalHashrate);
        String formattedAvgHashrate = formatHashrate(avgHashrate);
        String usernameStr = "User: " + String(result_balance_username);

        Serial.println("result_balance_username : " + String(result_balance_username));
        Serial.println("result_balance_balance : " + String(result_balance_balance));
        Serial.println("totalHashrate : " + formattedTotalHashrate);
        Serial.println("avgHashrate : " + formattedAvgHashrate);
        Serial.println("total_miner : " + String(total_miner));

        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB08_tr); // Choose a suitable font
        u8g2.drawStr(0, 10, "Duino-Coin Rig Stats");
        u8g2.drawStr(0, 30, usernameStr.c_str());
        u8g2.drawStr(0, 40, ("Coins: " + String(result_balance_balance) + " Duco").c_str());
        u8g2.drawStr(0, 50, ("Miners: " + String(total_miner)).c_str());
        u8g2.drawStr(0, 60, ("Hashrate: " + formattedTotalHashrate).c_str());
        u8g2.sendBuffer();
    }
}
