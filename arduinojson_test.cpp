#include <ArduinoJson.h>
#include <iostream>

int main() {
    JsonDocument eventMessage;
    JsonObject header = eventMessage["header"].to<JsonObject>();
    header["payloadVersion"] = 2;
    header["signatureVersion"] = 1;

    JsonObject payload = eventMessage["payload"].to<JsonObject>();
    payload["action"] = "currentTemperature";
    payload["cause"]["type"] = "PERIODIC_POLL";
    payload["createdAt"] = 0;
    payload["deviceId"] = "6a7186e509efd1746c350d10";
    payload["replyToken"] = "test-token-1234";
    payload["type"] = "event";
    
    JsonObject value = payload["value"].to<JsonObject>();
    value["humidity"] = 22.0;
    value["temperature"] = 22.0;

    std::string out;
    serializeJson(eventMessage, out);
    std::cout << "FULL:\n" << out << "\n";
    
    std::string payload_out;
    serializeJson(payload, payload_out);
    std::cout << "PAYLOAD ONLY:\n" << payload_out << "\n";

    return 0;
}
