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

// Conditional includes for Display
#ifdef BOARD_HAS_DISPLAY
#include <lvgl.h>
#include <TFT_eSPI.h>
/*Change to your screen resolution*/
static const uint16_t screenWidth = 240;
static const uint16_t screenHeight = 320;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf_1[screenWidth * 10]; /*Declare a buffer for 10 lines*/
#ifdef CONFIG_IDF_TARGET_ESP32S3 // S3 can use PSRAM for second buffer
static lv_color_t* buf_2 = NULL; // Will allocate in PSRAM
#else
static lv_color_t buf_2[screenWidth * 10]; /*Declare a buffer for 10 lines*/
#endif


TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); /* TFT instance */

#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char * buf)
{
    Serial.printf(buf);
    Serial.flush();
}
#endif

/* Display flushing */
void my_disp_flush( lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p )
{
    uint32_t w = ( area->x2 - area->x1 + 1 );
    uint32_t h = ( area->y2 - area->y1 + 1 );

    tft.startWrite();
    tft.setAddrWindow( area->x1, area->y1, w, h );
    tft.pushColors( ( uint16_t * )&color_p->full, w * h, true );
    tft.endWrite();

    lv_disp_flush_ready( disp_drv );
}

/*Read the touchpad*/
void my_touchpad_read( lv_indev_drv_t * indev_drv, lv_indev_data_t * data )
{
    uint16_t touchX, touchY;

    bool touched = tft.getTouch( &touchX, &touchY, 600 ); // 600 is pressure threshold

    if( !touched )
    {
        data->state = LV_INDEV_STATE_REL;
    }
    else
    {
        data->state = LV_INDEV_STATE_PR;

        /*Set the coordinates*/
        // Need to calibrate X and Y based on screen orientation and touch IC
        // For ESP32-2432S028R in portrait (240x320), XPT2046 might map like this:
        // X from ~200 (left) to ~3800 (right)
        // Y from ~200 (top) to ~3800 (bottom)
        // This needs calibration per device! The values below are placeholders.
        // data->point.x = map(touchX, 200, 3800, 0, screenWidth);
        // data->point.y = map(touchY, 200, 3800, 0, screenHeight);
        // For now, direct mapping, assuming TFT_eSPI handles rotation for touch.
        // If TFT_eSPI is set up for portrait, it might return coordinates already mapped to 240x320.
        data->point.x = touchX;
        data->point.y = touchY;


        // Serial.printf( "Touch X=%d, Y=%d -> Mapped X=%d, Y=%d\n", touchX, touchY, data->point.x, data->point.y );
    }
}

void lvgl_init() {
#if LV_USE_LOG != 0
    lv_log_register_print_cb( my_print ); /* register print function for debugging */
#endif

    lv_init();

    tft.begin();          /* TFT init */
    tft.setRotation( 0 ); /* Landscape orientation (default) or 1 for Portrait, 2 for Landscape inverted, 3 for Portrait inverted */
                          /* For 240x320 screen, usually rotation 0 or 1. If using 240 as width, 320 as height, this is likely portrait.
                             If screenWidth=320, screenHeight=240, then it's landscape.
                             The ESP32-2432S028R is 320 (width) x 240 (height) physically.
                             If we define screenWidth=240, screenHeight=320, we are using it in portrait.
                             Let's assume portrait: tft.setRotation(1) or (3).
                             If tft.setRotation(0) means physical 320x240, then LVGL hor_res=320, ver_res=240
                             Our lv_conf.h has hor_res=240, ver_res=320. So rotation should make TFT match this.
                             Commonly for ILI9341: 0=landscape(320x240), 1=portrait(240x320), 2=inv_landscape(320x240), 3=inv_portrait(240x320)
                             So, with screenWidth=240, screenHeight=320, we need tft.setRotation(1) or tft.setRotation(3).
                             Let's use 1 for now.
                           */
    tft.setRotation(1); // Portrait mode for 240x320 LVGL config

#ifdef CONFIG_IDF_TARGET_ESP32S3
    if (psramFound()) {
        buf_2 = (lv_color_t*)ps_malloc(screenWidth * 10 * sizeof(lv_color_t));
        if (!buf_2) {
            // Fallback to internal RAM if PSRAM allocation fails
            buf_2 = static_cast<lv_color_t*>(malloc(screenWidth * 10 * sizeof(lv_color_t)));
        }
    } else {
         buf_2 = static_cast<lv_color_t*>(malloc(screenWidth * 10 * sizeof(lv_color_t)));
    }
#endif // ifdef CONFIG_IDF_TARGET_ESP32S3 (for buf_2 allocation)
    if (!buf_1 || !buf_2) {
        Serial.println("LVGL disp buf alloc failed!");
        // Handle error, perhaps by reducing buffer size or stopping
        return;
    }
    lv_disp_draw_buf_init( &draw_buf, buf_1, buf_2, screenWidth * 10 );

    /*Initialize the display*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init( &disp_drv );
    /*Change the following line to your display resolution*/
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register( &disp_drv );

    /*Initialize the input device driver*/
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init( &indev_drv );
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register( &indev_drv );

    /* Get the active screen */
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr); // Clear screen, remove hello world label if any

    /* Create a container for the list of states */
    lv_obj_t *list_cont = lv_obj_create(scr);
    lv_obj_set_size(list_cont, screenWidth, screenHeight); // Use full screen
    lv_obj_align(list_cont, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_flex_flow(list_cont, LV_FLEX_FLOW_COLUMN); // Arrange items vertically
    lv_obj_set_flex_align(list_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    // lv_obj_set_layout(list_cont, LV_LAYOUT_FLEX); // Using flex for layout

    // Style for list items (optional, for padding etc.)
    static lv_style_t style_list_item;
    lv_style_init(&style_list_item);
    lv_style_set_pad_all(&style_list_item, 2); // Small padding around items
    lv_style_set_pad_gap(&style_list_item, 5); // Gap between elements in a row
    lv_style_set_width(&style_list_item, lv_pct(98)); // Make items take most of the width

    Serial.println("Populating OBD states on LVGL UI...");

    std::vector<OBDState *> states_to_display;
    OBD.getStates([](const OBDState *state) {
        return state->isVisible() && state->isEnabled();
    }, states_to_display);

    Serial.printf("Found %d states to display.\n", states_to_display.size());

    if (states_to_display.empty()) {
        lv_obj_t *label = lv_label_create(list_cont);
        lv_label_set_text(label, "No states to display.");
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    } else {
        for (OBDState *state : states_to_display) {
            // Container for each state row (description, value, unit)
            lv_obj_t *item_cont = lv_obj_create(list_cont);
            lv_obj_add_style(item_cont, &style_list_item, 0);
            lv_obj_set_flex_flow(item_cont, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(item_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_width(item_cont, lv_pct(95)); // Make item container take most of list_cont width

            // Description Label
            lv_obj_t *desc_label = lv_label_create(item_cont);
            lv_label_set_text(desc_label, state->getDescription());
            lv_label_set_long_mode(desc_label, LV_LABEL_LONG_WRAP);
            lv_obj_set_flex_grow(desc_label, 1); // Allow description to take available space
            lv_obj_set_style_text_align(desc_label, LV_TEXT_ALIGN_LEFT, 0);


            // Value Label
            lv_obj_t *val_label = lv_label_create(item_cont);
            char *val_str = nullptr;
            // Temporary value display - real value update will be in another step
            if (strcmp(state->valueType(), "int") == 0) {
                val_str = reinterpret_cast<TypedOBDState<int>*>(state)->formatValue();
            } else if (strcmp(state->valueType(), "float") == 0) {
                val_str = reinterpret_cast<TypedOBDState<float>*>(state)->formatValue();
            } else if (strcmp(state->valueType(), "bool") == 0) {
                val_str = reinterpret_cast<TypedOBDState<bool>*>(state)->formatValue();
            } else {
                val_str = strdup("N/A");
            }
            lv_label_set_text(val_label, val_str ? val_str : "Err");
            lv_obj_set_style_text_align(val_label, LV_TEXT_ALIGN_RIGHT, 0);
            if (val_str) free(val_str);
            obd_value_labels[state->getName()] = val_label; // Store the label pointer


            // Unit Label
            lv_obj_t *unit_label = lv_label_create(item_cont);
            lv_label_set_text(unit_label, state->getUnit());
            lv_obj_set_style_min_width(unit_label, 30); // Give unit some minimum space
            lv_obj_set_style_text_align(unit_label, LV_TEXT_ALIGN_RIGHT, 0);
            // obd_unit_labels[state->getName()] = unit_label; // If units can change


            Serial.printf("Added: %s (%s)\n", state->getDescription(), state->getName());
        }
    }
    Serial.println( "LVGL UI populated with states." );
}


#ifdef BOARD_HAS_DISPLAY
// Task to update LVGL labels with new OBD data
void lvgl_update_obd_task(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(1000); // Update every 1 second

    // Initialise the xLastWakeTime variable with the current time.
    xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        // Wait for the next cycle.
        vTaskDelayUntil( &xLastWakeTime, xFrequency );

        // Serial.println("LVGL OBD Update Task running...");
        for (auto const& [name, label_ptr] : obd_value_labels) {
            OBDState *state = OBD.getStateByName(name.c_str());
            if (state && state->isVisible() && state->isEnabled()) {
                char *val_str = nullptr;
                if (strcmp(state->valueType(), "int") == 0) {
                    val_str = reinterpret_cast<TypedOBDState<int>*>(state)->formatValue();
                } else if (strcmp(state->valueType(), "float") == 0) {
                    val_str = reinterpret_cast<TypedOBDState<float>*>(state)->formatValue();
                } else if (strcmp(state->valueType(), "bool") == 0) {
                    val_str = reinterpret_cast<TypedOBDState<bool>*>(state)->formatValue();
                } else {
                    // Should not happen if list is based on initially parsed states
                    val_str = strdup("N/A");
                }

                if (val_str) {
                    // Important: LVGL operations must be in LVGL task or protected by mutex
                    // For simplicity here, direct update. If issues, use lv_async_call or mutex
                    lv_label_set_text(label_ptr, val_str);
                    // Serial.printf("Updating %s to %s\n", name.c_str(), val_str);
                    free(val_str);
                }
            }
        }
    }
}
#endif

// Task to handle LVGL
void lvgl_handler_task(void *pvParameters) {
    for (;;) {
        lv_timer_handler(); /* let the GUI do its work */
        delay(5);
    }
}

#endif // BOARD_HAS_DISPLAY

#ifdef BOARD_HAS_DISPLAY
// For storing LVGL label objects that need updating
#include <map>
#include <string>
static std::map<std::string, lv_obj_t*> obd_value_labels;
static std::map<std::string, lv_obj_t*> obd_unit_labels; // If units could also change (less likely for now)
#endif

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
        DEBUG_PORT.printf("Found %d states to send\n", states.size());

        for (auto& state : states) {
            char tmp_char[50];
            if (state->getLastUpdate() + state->getUpdateInterval() > millis()) {
                continue;
            }

            DEBUG_PORT.printf("Sending state %s...\n", state->getName());

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

        //sendStaticDiagnosticData();
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

#ifdef BOARD_HAS_DISPLAY
    lvgl_init();
    // Create a task to run the LVGL handler. This task should have a higher priority than other UI related tasks.
    // LVGL recommends a stack size of at least 4KB for its handler if using complex UIs.
    // For now, 4096 should be fine. We can adjust if stack overflows occur.
    // Run it on core 1 if possible, to separate from WiFi/BT on core 0.
    // If readStatesTask is on core 1, we need to check affinities. Let's try core 0 for now.
    // Or, if stateTaskHdl is on core 1, and outputTaskHdl on core 0, then lvgl_handler_task can go on core 1.
    // Let's put LVGL on core 1, assuming network tasks might be heavier on core 0.
    // The `readStatesTask` is on core 1. `outputTask` is on core 0.
    // It's generally better to run UI on a different core from communication tasks.
    xTaskCreatePinnedToCore(
        lvgl_handler_task,
        "LVGLHandler",
        8192, // Increased stack for LVGL
        nullptr,
        5,  // Priority (ensure it's appropriate relative to other tasks)
        nullptr, // No task handle needed for now
        1 // Pin to core 1
    );
    DEBUG_PORT.println("LVGL task created.");

    // Create the LVGL OBD update task
    xTaskCreatePinnedToCore(
        lvgl_update_obd_task,
        "LVGLUpdateOBD",
        4096, // Stack size
        nullptr,
        4,    // Priority (lower than LVGLHandler, higher than some background tasks)
        nullptr, // No task handle needed
        1        // Pin to core 1 (same as LVGL handler or different, consider contention)
    );
    DEBUG_PORT.println("LVGL OBD Update task created.");
#endif
}

void loop() {
    // In FreeRTOS, loop() is usually empty or just deletes itself if setup creates all tasks.
    // If lv_timer_handler() is not in its own task, it would be called here.
    // But since we created lvgl_handler_task, this loop can remain empty.
    vTaskDelete(nullptr);
}
