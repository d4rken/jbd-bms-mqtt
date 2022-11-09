#include <../Config.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <Wire.h>

#include "JbdBms.h"

// for LED status
#include <Ticker.h>
Ticker ticker;

const char *SSID = WIFI_SSID;
const char *PSK = WIFI_PW;
const char *MQTT_BROKER = MQTT_SERVER;
const char *MQTT_BROKER_IP = MQTT_SERVER_IP;

WiFiClient espClient;
PubSubClient client(espClient);

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

JbdBms myBms(21,22);
unsigned long SerialLastLoad = 0;
packCellInfoStruct cellInfo;
float BatteryTemp1, BatteryTemp2, BatteryChargePercetage, BatteryCurrent,
    BatteryVoltage;
int BatterycycleCount;

void updateBMSData() {
    if (myBms.readBmsData() == true) {
        Serial.println("BMS data was read successfully");

        BatteryTemp1 = myBms.getTemp1();
        Serial.println("BatteryTemp1:");
        Serial.println(BatteryTemp1);

        BatteryCurrent = myBms.getCurrent() / 1000.0f;
        BatteryChargePercetage = myBms.getChargePercentage();
        
        BatteryTemp2 = myBms.getTemp2();
        BatterycycleCount = myBms.getCycle();
        BatteryVoltage = myBms.getVoltage();
        if (myBms.readPackData() == true) {
            cellInfo = myBms.getPackCellInfo();
        }
    } else {
        Serial.println("BMS data was read successfully");
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
