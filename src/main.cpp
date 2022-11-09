#include <../Config.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#include "JbdBms.h"
#include <Ticker.h>

Ticker ticker;

const char *SSID = WIFI_SSID;
const char *PSK = WIFI_PW;
const char *MQTT_BROKER = MQTT_SERVER;
const char *MQTT_BROKER_IP = MQTT_SERVER_IP;

WiFiClient espClient;
PubSubClient client(espClient);

SoftwareSerial mySerial(D2, D1);
JbdBms myBms(&mySerial);

void tick() {
    // toggle state
    int state = digitalRead(LED_BUILTIN);  // get the current state of GPIO1 pin
    digitalWrite(LED_BUILTIN, !state);     // set pin to the opposite state
}

void setup() {
    Serial.begin(74880);
    Serial.println("Setup...");

    pinMode(LED_BUILTIN, OUTPUT);
    ticker.attach(0.6, tick);

    Serial.println("Setting up WIFI" + String(SSID));
    WiFi.setAutoReconnect(true);
    WiFi.hostname("Weather-Station-Outdoor");
    WiFi.begin(SSID, PSK);

    client.setServer(MQTT_BROKER, 1883);

    pinMode(A0, INPUT);
}

void updateSystemStats() {
    long rssi = WiFi.RSSI();
    Serial.println("RSSI: " + String(rssi));
}

void updateBMSData() {
  if (myBms.readBmsData() == true) {
        Serial.print("This capacity: ");
        Serial.println(myBms.getChargePercentage());
        Serial.print("Current: ");
        Serial.println(myBms.getCurrent());
        Serial.print("Voltage: ");
        Serial.println(myBms.getVoltage());
        Serial.print("Protection state: ");
        Serial.println(myBms.getProtectionState());
        Serial.print("Cycle: ");
        Serial.println(myBms.getCycle());
        Serial.print("Temp1: ");
        Serial.println(myBms.getTemp1());
        Serial.print("Temp2: ");
        Serial.println(myBms.getTemp2());
        Serial.print("Temp3: ");
        Serial.println(myBms.getTemp3());

        Serial.println();
    } else {
        Serial.println("Communication error");
    }
    delay(3000);
    if (myBms.readPackData() == true) {
        packCellInfoStruct packInfo = myBms.getPackCellInfo();

        Serial.print("Number Of Cell: ");
        Serial.println(packInfo.NumOfCells);
        Serial.print("Low: ");
        Serial.println(packInfo.CellLow);
        Serial.print("High: ");
        Serial.println(packInfo.CellHigh);
        Serial.print("Diff: ");
        Serial.println(packInfo.CellDiff);
        Serial.print("Avg: ");
        Serial.println(packInfo.CellAvg);

        // go trough individual cells
        for (byte i = 0; i < packInfo.NumOfCells; i++) {
            Serial.print("Cell");
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.println(packInfo.CellVoltage[i]);
        }
        Serial.println();
    } else {
        Serial.println("Communication error");
    }
}

void loop() {
    Serial.println("Loop start");

    bool wifiConnected = true;
    int retryWifi = 0;
    while (WiFi.status() != WL_CONNECTED) {
        Serial.println("Connecting to WiFi...");
        if (retryWifi > 10) {
            wifiConnected = false;
            break;
        } else {
            retryWifi++;
        }
        delay(500);
    }

    if (wifiConnected) {
        Serial.println("Connected, my IP is: " + WiFi.localIP().toString());
    } else {
        Serial.println("Failed to connect to WiFi");
    }

    bool mqttConnected = wifiConnected;
    int retryMqtt = 0;
    while (wifiConnected && !client.connected()) {
        Serial.println("Connecting to MQTT broker...");
        client.connect("JBD-BMS-1");
        if (retryMqtt > 3) {
            mqttConnected = false;
            break;
        } else {
            if (retryMqtt == 2) {
                Serial.println("MQTT DNS not resolved, trying IP...");
                client.disconnect();
                client.setServer(MQTT_BROKER_IP, 1883);
            }
            retryMqtt++;
        }
        delay(500);
    }

    if (mqttConnected) {
        Serial.println("Connected to MQTT Broker");
    } else {
        Serial.println("Failed to connect to MQTT broker");
    }

    ticker.detach();
    digitalWrite(BUILTIN_LED, LOW);

    Serial.println("Wifi and MQTT ready");

    updateSystemStats();
    updateBMSData();

    client.loop();

    Serial.println("Loop end");

    delay(5000);
}
