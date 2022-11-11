# jbd-bms-mqtt

Use an ESP8266 to send data from a JBD BMS to a MQTT broker.

The JBD BMS communication is a fork from @rahmaevao's [JbdBms library](https://github.com/rahmaevao/JbdBms)

I adjusted the message length for the JBD BMS `AP21S001`:
https://jiabaidabms.com/en-de/products/jbd-smart-bms-8s-24v-100a-lithium-battery-protection-circuit-board-with-passive-balance-temp-sensor

The rest of the code for WiFi and MQTT is based on my [weather-station](https://github.com/d4rken/arduino-weather-station-v2).