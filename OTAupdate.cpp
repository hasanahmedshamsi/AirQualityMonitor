#include "OTAUpdate.h"

void setupStream() {
    if (!Firebase.RTDB.beginStream(&fbdoStream, "/Firmware")) {
        Serial.println("Failed to begin Stream: " + fbdoStream.errorReason());
    }
    Firebase.RTDB.setStreamCallback(&fbdoStream, streamCallback, streamTimeoutCallback);
}

void streamCallback(FirebaseStream data) {
    Serial.println("Event received");

    if (data.dataTypeEnum() == fb_esp_rtdb_data_type_boolean && Firebase.ready()) {
        bool updateFlag = data.boolData();
        Serial.print("Update flag: ");
        Serial.println(updateFlag ? "TRUE" : "FALSE");

        if (updateFlag) {
            Serial.println("Update triggered!");

            if(Firebase.ready()){
                if(Firebase.RTDB.getString(&fbdo, "Firmware/url")){
                    if(fbdo.dataType() == "string"){
                        url = fbdo.stringData();
                        Serial.println("Read URL: " + url);
                        updateFirmware();
                    }
                } else {
                    Serial.println("Failed: " + fbdo.errorReason());
                }
            }
        }
    }
}

void streamTimeoutCallback(bool timeout) {
    if (timeout) {
        Serial.println("Stream timed out, restarting stream...");
        Firebase.RTDB.endStream(&fbdoStream);
        Firebase.RTDB.beginStream(&fbdoStream, "/Firmware");
    }
}


void updateFirmware() {
    otaInProgress = true;
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); 

    int responseCode = http.GET();

    if (Firebase.ready()) {
        Firebase.RTDB.setString(&fbdo, "/Firmware/status", "Starting OTA update");
        delay(200);
    }

    if (responseCode == HTTP_CODE_OK) {
        int length = http.getSize();
        bool beginUpdate = Update.begin(length);

        if (beginUpdate) {
            Firebase.RTDB.setString(&fbdo, "/Firmware/status", "Writing firmware...");
            delay(200);

            Serial.println("Starting Firmware Update");
            WiFiClient *stream = http.getStreamPtr();
            size_t written = Update.writeStream(*stream);

            if (written == length) {
                Firebase.RTDB.setString(&fbdo, "/Firmware/status", "Firmware written successfully");
                Serial.println("Firmware written successfully");
            } else {
                Firebase.RTDB.setString(&fbdo, "/Firmware/status", "Partial write: " + String(written) + "/" + String(length));
                Serial.println("Written only " + String(written) + "/" + String(length) + " bytes");
            }
            delay(200);

            if (Update.end()) {
                if (Update.isFinished()) {
                    Firebase.RTDB.setString(&fbdo, "/Firmware/status", "Update complete, rebooting");
                    Firebase.RTDB.setBool(&fbdo, "/Firmware/update", false);
                    delay(500);
                    Serial.println("Update Completed. Rebooting...");
                    ESP.restart();
                } else {
                    Firebase.RTDB.setString(&fbdo, "/Firmware/status", "Update not finished");
                    Serial.println("Update not finished");
                }
            } else {
                Serial.printf("Error in Update: %s\n", Update.errorString());
                Firebase.RTDB.setString(&fbdo, "/Firmware/status", "Error: " + String(Update.errorString()));
            }

        } else {
            Firebase.RTDB.setString(&fbdo, "/Firmware/status", "Not enough space for update");
            Serial.println("Not enough space");
        }
    } else {
        Serial.println("HTTP error: " + String(responseCode));
        Firebase.RTDB.setString(&fbdo, "/Firmware/status", "HTTP error: " + String(responseCode));
    }

    delay(200);
    otaInProgress = false;
}
