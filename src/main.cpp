/*
 * This program is free software; you can use it, redistribute it
 * and / or modify it under the terms of the GNU General Public License
 * (GPL) as published by the Free Software Foundation; either version 3
 * of the License or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program, in a file called gpl.txt or license.txt.
 * If not, write to the Free Software Foundation Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307 USA
 */

#include <WiFi.h>
#include <atomic>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif
#if !defined(CONFIG_BT_SPP_ENABLED) && !defined(USE_BLE)
#error Serial Bluetooth not available or not enabled. It is only available for the ESP32 chip.
#endif

#ifndef BUILD_GIT_BRANCH
#define BUILD_GIT_BRANCH ""
#endif
#ifndef BUILD_GIT_COMMIT_HASH
#define BUILD_GIT_COMMIT_HASH ""
#endif

#define MIN_VOLTAGE_LEVEL       3300
#define LOW_VOLTAGE_LEVEL       3600            // Sleep shutdown voltage

#include <LittleFS.h>

#define FORMAT_LITTLEFS_IF_FAILED true

#define DISCOVERED_DEVICES_FILE "/discovered_devices.json"

#include <numeric>

#include "settings.h"
#include "helper.h"
#include "obd.h"
#include "http.h"

HTTPServer server(80);

#define DEBUG_PORT Serial

// #define DUMP_AT_COMMANDS

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, Serial);
GSM gsm(debugger);
#endif


std::atomic_bool wifiAPStarted{ false };
std::atomic_bool wifiAPInUse{ false };
std::atomic<unsigned int> wifiAPStaConnected{ 0 };

std::atomic<int> obdConnectErrors{ 0 };

std::atomic<unsigned long> startTime{ 0 };

TaskHandle_t outputTaskHdl;
TaskHandle_t stateTaskHdl;

size_t getESPHeapSize() {
    return heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

void deepSleep(const int sec) {
    DEBUG_PORT.println("Prepare nap...");
    WiFi.disconnect(true);
    OBD.end();
    if (outputTaskHdl != nullptr) {
        vTaskDelete(outputTaskHdl);
    }
    if (stateTaskHdl != nullptr) {
        vTaskDelete(stateTaskHdl);
    }
    DEBUG_PORT.println("...ZzZzZz.");
}

String getVersion() {
    auto version = String(BUILD_GIT_BRANCH);
    if (!version.startsWith("v")) {
        version += " (" + String(BUILD_GIT_COMMIT_HASH) + ")";
    }
    return version;
}

void WiFiAPStart(WiFiEvent_t event, WiFiEventInfo_t info) {
    wifiAPStarted = true;
    DEBUG_PORT.println("WiFi AP started.");

    DEBUG_PORT.printf("AP - IP address: %s\n", WiFi.softAPIP().toString().c_str());
}

void WiFiAPStop(WiFiEvent_t event, WiFiEventInfo_t info) {
    wifiAPStarted = false;
    wifiAPInUse = false;
    wifiAPStaConnected = 0;
    DEBUG_PORT.println("WiFi AP stopped.");
}

void WiFiAPStationConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    ++wifiAPStaConnected;
    wifiAPInUse = true;

    if (wifiAPStaConnected == 1) {
        DEBUG_PORT.println("WiFi AP in use. Stop all other task.");
        OBD.end();
    }
}

void WiFiAPStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (wifiAPStaConnected != 0) {
        --wifiAPStaConnected;
    }

    if (wifiAPStaConnected == 0) {
        DEBUG_PORT.println("WiFi AP all clients disconnected. Start all other task.");
        OBD.begin(Settings.OBD2.getName(OBD_ADP_NAME), Settings.OBD2.getMAC(), Settings.OBD2.getProtocol(),
            Settings.OBD2.getCheckPIDSupport(), Settings.OBD2.getDebug(), Settings.OBD2.getSpecifyNumResponses());
        OBD.connect(true);
        wifiAPInUse = false;
    }
}

void startWiFiAP() {
    DEBUG_PORT.print("Start Access Point...");

    WiFi.disconnect(true);

    WiFi.mode(WIFI_AP);

    WiFi.onEvent(WiFiAPStart, WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_START);
    WiFi.onEvent(WiFiAPStop, WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STOP);
    WiFi.onEvent(WiFiAPStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STACONNECTED);
    WiFi.onEvent(WiFiAPStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);

    String ssid = Settings.WiFi.getAPSSID();
    if (ssid.isEmpty()) {
        ssid = "OBD2-MQTT-" + String(stripChars(WiFi.macAddress().c_str()).c_str());
        Settings.WiFi.setAPSSID(ssid.c_str());
    }
    WiFi.softAP(
        ssid.c_str(),
        Settings.WiFi.getAPPassword()
    );
}

void startHttpServer() {
    server.on("/api/version", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", getVersion());
        });

    server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "application/json", Settings.buildJson().c_str());
        });

    server.on(
        "/api/settings",
        HTTP_PUT,
        [](AsyncWebServerRequest* request) {
        },
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (request->contentType() == "application/json") {
                if (!index) {
                    request->_tempObject = malloc(total);
                }

                if (request->_tempObject != nullptr) {
                    memcpy(static_cast<uint8_t*>(request->_tempObject) + index, data, len);

                    if (index + len == total) {
                        auto json = std::string(static_cast<const char*>(request->_tempObject), total);
                        if (Settings.parseJson(json)) {
                            if (Settings.writeSettings(LittleFS)) {
                                request->send(200);
                            }
                        } else {
                            request->send(500);
                        }
                    }
                }
            } else {
                request->send(406);
            }
        }
    );

    server.on("/api/states", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "application/json", OBD.buildJSON().c_str());
        });

    server.on(
        "/api/states",
        HTTP_PUT,
        [](AsyncWebServerRequest* request) {
        },
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (request->contentType() == "application/json") {
                if (!index) {
                    request->_tempObject = malloc(total);
                }

                if (request->_tempObject != nullptr) {
                    memcpy(static_cast<uint8_t*>(request->_tempObject) + index, data, len);

                    if (index + len == total) {
                        auto json = std::string(static_cast<const char*>(request->_tempObject), total);
                        if (OBD.parseJSON(json)) {
                            if (OBD.writeStates(LittleFS)) {
                                request->send(200);
                            }
                        } else {
                            request->send(500);
                        }
                    }
                }
            } else {
                request->send(406);
            }
        }
    );

    server.on("/api/wifi", HTTP_GET, [](AsyncWebServerRequest* request) {
        std::string payload;
        JsonDocument wifiInfo;

        wifiInfo["hostname"] = WiFi.softAPgetHostname();
        wifiInfo["SSID"] = WiFi.softAPSSID();
        wifiInfo["ip"] = WiFi.softAPIP().toString();
        wifiInfo["mac"] = WiFi.macAddress();

        serializeJson(wifiInfo, payload);

        request->send(200, "application/json", payload.c_str());
        });

    server.on("/api/discoveredDevices", HTTP_GET, [](AsyncWebServerRequest* request) {
        File file = LittleFS.open(DISCOVERED_DEVICES_FILE, FILE_READ);
        if (file && !file.isDirectory()) {
            JsonDocument doc;
            if (!deserializeJson(doc, file)) {
                std::string payload;
                serializeJson(doc, payload);
                request->send(200, "application/json", payload.c_str());
            } else {
                request->send(500);
            }
            file.close();
        } else {
            request->send(404);
        }
        });

    server.on("/api/DTCs", HTTP_GET, [](AsyncWebServerRequest* request) {
        DTCs* dtcs = OBD.getDTCs();
        if (dtcs->getCount() != 0) {
            JsonDocument doc;
            for (int i = 0; i < dtcs->getCount(); ++i) {
                doc["dtc"].add(dtcs->getCode(i)->c_str());
            }
            std::string payload;
            if (serializeJson(doc, payload)) {
                request->send(200, "application/json", payload.c_str());
            } else {
                request->send(500);
            }
        } else {
            request->send(404);
        }
        });

    server.begin(LittleFS);
}

void onOBDConnected() {
    obdConnectErrors = 0;
}

void onOBDConnectError() {
    ++obdConnectErrors;
    if (obdConnectErrors > 5) {
        deepSleep(Settings.General.getSleepDuration());
    }
}

#ifdef USE_BLE
void onBLEDevicesDiscovered(BLEScanResultsSet* btDeviceList) {
    JsonDocument devices;

    File file = LittleFS.open(DISCOVERED_DEVICES_FILE, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file discovered_devices.json for writing.");
    }

    for (int i = 0; i < btDeviceList->getCount(); i++) {
        JsonDocument dev;
        BLEAdvertisedDevice* device = btDeviceList->getDevice(i);
        if (device && !device->getName().empty()) {
            dev["name"] = device->getName();
            dev["mac"] = device->getAddress().toString();
            devices["device"].add(dev);
        }
    }

    serializeJson(devices, file);

    file.close();
}

#else
void onBTDevicesDiscovered(BTScanResults* btDeviceList) {
    JsonDocument devices;

    File file = LittleFS.open(DISCOVERED_DEVICES_FILE, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file discovered_devices.json for writing.");
    }

    for (int i = 0; i < btDeviceList->getCount(); i++) {
        JsonDocument dev;
        BTAdvertisedDevice* device = btDeviceList->getDevice(i);
        dev["name"] = device->getName();
        dev["mac"] = device->getAddress().toString();
        devices["device"].add(dev);
    }

    serializeJson(devices, file);

    file.close();
}
#endif

bool sendOBDData() {
    const unsigned long start = millis();
    bool allSendsSucceeded = false;

    DEBUG_PORT.print("Send OBD data...");

    // allSendsSucceeded |= mqtt.sendTopicUpdate(LWT_TOPIC, LWT_CONNECTED);

    std::vector<OBDState*> states{};
    OBD.getStates([](const OBDState* state) {
        return state->isVisible() && state->isEnabled() && state->isSupported() && !(
            state->isDiagnostic() && state->getUpdateInterval() == -1);
        }, states);
    if (!states.empty()) {
        for (auto& state : states) {
            char tmp_char[50];
            if (state->getLastUpdate() + state->getUpdateInterval() > millis()) {
                continue;
            }

            if (state->valueType() == "int") {
                auto* is = reinterpret_cast<OBDStateInt*>(state);
                char* str = is->formatValue();
                strcpy(tmp_char, str);
                free(str);
            } else if (state->valueType() == "float") {
                auto* is = reinterpret_cast<OBDStateFloat*>(state);
                char* str = is->formatValue();
                strcpy(tmp_char, str);
                free(str);
            } else if (state->valueType() == "bool") {
                auto* is = reinterpret_cast<OBDStateBool*>(state);
                char* str = is->formatValue();
                strcpy(tmp_char, str);
                free(str);
            }

            DEBUG_PORT.printf("State %s: %s", state->getName(), std::string(tmp_char));

            // allSendsSucceeded |= mqtt.sendTopicUpdate(state->getName(), std::string(tmp_char));
        }
    } else {
        allSendsSucceeded = true;
    }

    DEBUG_PORT.printf("...%s (%dms)\n", allSendsSucceeded ? "done" : "failed", millis() - start);

    return allSendsSucceeded;
}

bool sendDiagnosticData() {
    const unsigned long start = millis();
    bool allSendsSucceeded = false;
    char tmp_char[50];

    DEBUG_PORT.print("Send diagnostic data...");

    sprintf(tmp_char, "%d", static_cast<int>(temperatureRead()));
    DEBUG_PORT.printf("Temperature: %s\n", tmp_char);
    // allSendsSucceeded |= mqtt.sendTopicUpdate("cpuTemp", std::string(tmp_char));

    sprintf(tmp_char, "%lu", static_cast<long>(getESPHeapSize()));
    DEBUG_PORT.printf("Heap size: %s\n", tmp_char);
    // allSendsSucceeded |= mqtt.sendTopicUpdate("freeMem", std::string(tmp_char));

    sprintf(tmp_char, "%lu", (millis() - startTime) / 1000);
    DEBUG_PORT.printf("Uptime: %s\n", tmp_char);
    // allSendsSucceeded |= mqtt.sendTopicUpdate("uptime", std::string(tmp_char));

    DEBUG_PORT.printf("...%s (%dms)\n", allSendsSucceeded ? "done" : "failed", millis() - start);

    return allSendsSucceeded;
}

bool sendStaticDiagnosticData() {
    const unsigned long start = millis();
    bool allSendsSucceeded = false;

    DEBUG_PORT.print("Send static diagnostic data...");

    std::vector<OBDState*> states{};
    OBD.getStates([](const OBDState* state) {
        return state->isVisible() && state->isEnabled() && state->isSupported() && state->isDiagnostic() && state->
            getUpdateInterval() == -1;
        }, states);
    if (!states.empty()) {
        for (auto& state : states) {
            char tmp_char[50];
            if (state->getLastUpdate() + state->getUpdateInterval() > millis()) {
                continue;
            }

            if (state->valueType() == "int") {
                auto* is = reinterpret_cast<OBDStateInt*>(state);
                char* str = is->formatValue();
                strcpy(tmp_char, str);
                free(str);
            } else if (state->valueType() == "float") {
                auto* is = reinterpret_cast<OBDStateFloat*>(state);
                char* str = is->formatValue();
                strcpy(tmp_char, str);
                free(str);
            } else if (state->valueType() == "bool") {
                auto* is = reinterpret_cast<OBDStateBool*>(state);
                char* str = is->formatValue();
                strcpy(tmp_char, str);
                free(str);
            }

            DEBUG_PORT.printf("State %s: %s\n", state->getName(), std::string(tmp_char));

            // allSendsSucceeded |= mqtt.sendTopicUpdate(state->getName(), std::string(tmp_char));
        }
    } else {
        allSendsSucceeded = true;
    }

    DEBUG_PORT.printf("...%s (%dms)\n", allSendsSucceeded ? "done" : "failed", millis() - start);

    return allSendsSucceeded;
}

[[noreturn]] void readStatesTask(void* parameters) {
    for (;;) {
        if (!wifiAPInUse) {
            OBD.loop();
        }
        delay(10);
    }
}

[[noreturn]] void outputTask(void* parameters) {
    for (;;) {

        if (wifiAPInUse) {
            delay(1000);
            continue;
        }

        sendStaticDiagnosticData();
        sendDiagnosticData();
        sendOBDData();

        delay(2000);
    }
}

void setup() {
    startTime = millis();

    DEBUG_PORT.begin(115200);

    if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
        DEBUG_PORT.println("LittleFS Mount Failed");
        return;
    }

    // Test LittleFS and print free space
    DEBUG_PORT.printf("LittleFS free space: %d bytes\n", LittleFS.totalBytes() - LittleFS.usedBytes());
    DEBUG_PORT.printf("LittleFS used space: %d bytes\n", LittleFS.usedBytes());
    DEBUG_PORT.printf("LittleFS total space: %d bytes\n", LittleFS.totalBytes());

    bool success = false;

    DEBUG_PORT.println("Reading settings...");
    success = Settings.readSettings(LittleFS);
    DEBUG_PORT.printf("Settings read %s\n", success ? "success" : "failed");

    DEBUG_PORT.println("Reading OBD states...");
    success = OBD.readStates(LittleFS);
    DEBUG_PORT.printf("OBD states read %s\n", success ? "success" : "failed");

    // disable Watch Dog for Core 0 - should fix crashes
    disableCore0WDT();

    startWiFiAP();
    startHttpServer();

    OBD.onConnected(onOBDConnected);
    OBD.onConnectError(onOBDConnectError);
    OBD.begin(Settings.OBD2.getName(OBD_ADP_NAME), Settings.OBD2.getMAC(), Settings.OBD2.getProtocol(),
        Settings.OBD2.getCheckPIDSupport(), Settings.OBD2.getDebug(), Settings.OBD2.getSpecifyNumResponses());
#ifdef USE_BLE
    OBD.onDevicesDiscovered(onBLEDevicesDiscovered);
#else
    OBD.onDevicesDiscovered(onBTDevicesDiscovered);
#endif
    OBD.connect();

    xTaskCreatePinnedToCore(outputTask, "OutputTask", 9216, nullptr, 10, &outputTaskHdl, 0);

    xTaskCreatePinnedToCore(readStatesTask, "ReadStatesTask", 9216, nullptr, 1, &stateTaskHdl, 1);
}

void loop() {
    vTaskDelete(nullptr);
}
