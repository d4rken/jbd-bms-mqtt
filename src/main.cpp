#include <../Config.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include <Ticker.h>
#include <Wire.h>

#include "JbdBms.h"

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
        Serial.println("### START: Basic BMS data ###");

        float chargePercentage = myBms.getChargePercentage();
        Serial.println("This capacity: " + String(chargePercentage));

        float current = myBms.getCurrent();
        Serial.println("Current: " + String(current));

        float voltage = myBms.getVoltage();
        Serial.println("Voltage: " + String(voltage));

        uint16_t protectionStation = myBms.getProtectionState();
        Serial.println("Protection state: " + String(protectionStation));

        uint16_t cycleCount = (myBms.getCycle());
        Serial.println("Cycle: " + String(cycleCount));

        float tempInternal = myBms.getTemp1();
        Serial.println("Temp internal: " + String(tempInternal));

        float tempProbe1 = myBms.getTemp2();
        Serial.println("Temp probe 1: " + String(tempProbe1));

        float tempProbe2 = myBms.getTemp3();
        Serial.println("Temp probe 2: " + String(tempProbe2));

        Serial.println("### END: Basic BMS data ###");
    } else {
        Serial.println("Communication error while getting basic BMS data");
    }
    delay(3000);
    if (myBms.readPackData() == true) {
        Serial.println("### START: Battery cell data ###");

        packCellInfoStruct packInfo = myBms.getPackCellInfo();

        Serial.println("Number Of Cell: " + String(packInfo.NumOfCells));
        Serial.println("Low: " + String(packInfo.CellLow));
        Serial.println("High: " + String(packInfo.CellHigh));
        Serial.println("Diff: " + String(packInfo.CellDiff));
        Serial.println("Avg: " + String(packInfo.CellAvg));

        // go trough individual cells
        for (byte i = 0; i < packInfo.NumOfCells; i++) {
            Serial.print("Cell");
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.println(packInfo.CellVoltage[i]);
        }

        Serial.println("### END: Battery cell data ###");
    } else {
        Serial.println("Communication error while getting cell data");
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
