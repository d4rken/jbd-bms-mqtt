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
    WiFi.hostname("PV1-BMS1-ESP8266");
    WiFi.begin(SSID, PSK);

    client.setServer(MQTT_BROKER, 1883);

    pinMode(A0, INPUT);
}

char itoaBuf[64];
char dtostrfBuf[64];

void updateSystemStats() {
    long rssi = WiFi.RSSI();
    Serial.println("RSSI: " + String(rssi));
    client.publish("pv1/bms1/rssi", itoa(rssi, itoaBuf, 10));

    unsigned long uptime = millis();
    Serial.println("Uptime: " + String(uptime));
    client.publish("pv1/bms1/uptime", itoa(uptime, itoaBuf, 10));
}

void updateBMSData() {
    int attempts = 0;

    bool hasBmsData = false;
    while (!hasBmsData) {
        if (attempts > 8) break;
        Serial.println("Trying for bms data");
        hasBmsData = myBms.readBmsData();
        delay(200);
        attempts++;
    }
    if (hasBmsData) {
        Serial.println("### START: Basic BMS data ###");

        float chargePercentage = myBms.getChargePercentage();
        Serial.println("Charge percentage capacity: " + String(chargePercentage));
        client.publish("pv1/bms1/charge", dtostrf(chargePercentage, 2, 2, dtostrfBuf));

        float current = myBms.getCurrent();
        Serial.println("Current: " + String(current));
        client.publish("pv1/bms1/current", itoa(current, itoaBuf, 10));

        float voltage = myBms.getVoltage();
        Serial.println("Voltage: " + String(voltage));
        client.publish("pv1/bms1/voltage", dtostrf(voltage, 2, 2, dtostrfBuf));

        uint16_t protectionState = myBms.getProtectionState();
        Serial.println("Protection state: " + String(protectionState));
        client.publish("pv1/bms1/protection-state", itoa(protectionState, itoaBuf, 10));

        uint16_t cycleCount = (myBms.getCycle());
        Serial.println("Cycle: " + String(cycleCount));
        client.publish("pv1/bms1/cycle-count", itoa(cycleCount, itoaBuf, 10));

        float tempInternal = myBms.getTemp1();
        Serial.println("Temp internal: " + String(tempInternal));
        client.publish("pv1/bms1/temp-internal", dtostrf(tempInternal, 2, 2, dtostrfBuf));

        float tempProbe1 = myBms.getTemp2();
        Serial.println("Temp probe 1: " + String(tempProbe1));
        client.publish("pv1/bms1/temp-probe1", dtostrf(tempProbe1, 2, 2, dtostrfBuf));

        float tempProbe2 = myBms.getTemp3();
        Serial.println("Temp probe 2: " + String(tempProbe2));
        client.publish("pv1/bms1/temp-probe2", dtostrf(tempProbe2, 2, 2, dtostrfBuf));

        Serial.println("### END: Basic BMS data ###");
    } else {
        Serial.println("Communication error while getting basic BMS data");
    }

    attempts = 0;

    bool hasPackData = false;
    while (!hasPackData) {
        if (attempts > 8) break;
        Serial.println("Trying for pack data");
        hasPackData = myBms.readPackData();
        delay(200);
        attempts++;
    }
    if (hasPackData) {
        Serial.println("### START: Battery cell data ###");

        packCellInfoStruct packInfo = myBms.getPackCellInfo();

        Serial.println("Number Of Cell: " + String(packInfo.NumOfCells));
        client.publish("pv1/bms1/cells/count", itoa(packInfo.NumOfCells, itoaBuf, 10));

        Serial.println("Low: " + String(packInfo.CellLow));
        client.publish("pv1/bms1/cells/low", itoa(packInfo.CellLow, itoaBuf, 10));

        Serial.println("High: " + String(packInfo.CellHigh));
        client.publish("pv1/bms1/cells/high", itoa(packInfo.CellHigh, itoaBuf, 10));

        Serial.println("Diff: " + String(packInfo.CellDiff));
        client.publish("pv1/bms1/cells/diff", itoa(packInfo.CellDiff, itoaBuf, 10));

        Serial.println("Avg: " + String(packInfo.CellAvg));
        client.publish("pv1/bms1/cells/avg", itoa(packInfo.CellAvg, itoaBuf, 10));

        // go trough individual cells
        for (byte i = 0; i < packInfo.NumOfCells; i++) {
            Serial.println("Cell #" + String(i + 1) + ": " + packInfo.CellVoltage[i]);
            client.publish(("pv1/bms1/cells/cell" + String(i + 1)).c_str(), itoa(packInfo.CellVoltage[i], itoaBuf, 10));
        }

        Serial.println("### END: Battery cell data ###");
    } else {
        Serial.println("Communication error while getting cell data");
    }
}

void loop() {
    Serial.println("Loop start");
    digitalWrite(LED_BUILTIN, LOW);

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
        client.connect("BMS-JBD-1");
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
    digitalWrite(LED_BUILTIN, LOW);

    Serial.println("Wifi and MQTT ready");

    delay(2000);

    updateSystemStats();

    updateBMSData();

    client.loop();

    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("Loop end");

    delay(27000);
}
