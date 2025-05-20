/*
 * This program is free software; you can use it, redistribute it
 * and / or modify it under the terms of the GNU General Public License
 * (GPL) as published by the Free Software Foundation; either version 3
 * of the License or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program, in a file called gpl.txt or license.txt.
 *  If not, write to the Free Software Foundation Inc.,
 *  59 Temple Place - Suite 330, Boston, MA  02111-1307 USA
 */
#include "settings.h"

#include <FS.h>
#include <ArduinoJson.h>

SettingsClass::SettingsClass() = default;

void SettingsClass::readJson(JsonDocument &doc) {
    General.readJson(doc);
    WiFi.readJson(doc);
    OBD2.readJson(doc);
}

void SettingsClass::writeJson(JsonDocument &doc) {
    General.writeJson(doc);
    WiFi.writeJson(doc);
    OBD2.writeJson(doc);
}

bool SettingsClass::readSettings(fs::FS &fs) {
    bool success = false;

    File file = fs.open(SETTINGS_FILE, FILE_READ);
    if (file && !file.isDirectory()) {
        JsonDocument doc;
        if (!deserializeJson(doc, file)) {
            readJson(doc);
            success = true;
        }
        file.close();
    }

    return success;
}

bool SettingsClass::writeSettings(fs::FS &fs) {
    bool success = false;

    File file = fs.open(SETTINGS_FILE, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file settings.json for writing.");
        return false;
    }

    JsonDocument doc;
    writeJson(doc);
    success = serializeJson(doc, file);

    file.close();

    return success;
}

std::string SettingsClass::buildJson() {
    std::string payload;

    JsonDocument doc;
    writeJson(doc);
    serializeJson(doc, payload);

    return payload;
}

bool SettingsClass::parseJson(std::string json) {
    bool success = false;
    JsonDocument doc;
    if (!deserializeJson(doc, json)) {
        readJson(doc);
        success = true;
    }

    return success;
}

void GeneralSettings::readJson(JsonDocument &doc) {
    general.sleepTimeout = doc["general"]["sleepTimeout"] | 5 * 60;
    general.sleepDuration = doc["general"]["sleepDuration"] | 60 * 60;
}

void GeneralSettings::writeJson(JsonDocument &doc) {
    doc["general"]["sleepTimeout"] = general.sleepTimeout;
    doc["general"]["sleepDuration"] = general.sleepDuration;
}

int GeneralSettings::getSleepTimeout() const {
    return general.sleepTimeout;
}

void GeneralSettings::setSleepTimeout(const int timeout) {
    general.sleepTimeout = timeout;
}

int GeneralSettings::getSleepDuration() const {
    return general.sleepDuration;
}

void GeneralSettings::setSleepDuration(const int time) {
    general.sleepDuration = time;
}

void WiFiSettings::readJson(JsonDocument &doc) {
    strlcpy(wifi.ssid, doc["wifi"]["ssid"] | "", sizeof(wifi.ssid));
    strlcpy(wifi.password, doc["wifi"]["password"] | "", sizeof(wifi.password));
}

void WiFiSettings::writeJson(JsonDocument &doc) {
    doc["wifi"]["ssid"] = wifi.ssid;
    doc["wifi"]["password"] = wifi.password;
}

String WiFiSettings::getAPSSID(const String &alternate) const {
    if (strlen(wifi.ssid) == 0) {
        return alternate;
    }
    return wifi.ssid;
}

void WiFiSettings::setAPSSID(const char *ssid) {
    strlcpy(wifi.ssid, ssid, sizeof(wifi.ssid));
}

String WiFiSettings::getAPPassword() const {
    return wifi.password;
}

void WiFiSettings::setAPPassword(const char *password) {
    strlcpy(wifi.password, password, sizeof(wifi.password));
}

void OBD2Settings::readJson(JsonDocument &doc) {
    strlcpy(obd2.name, doc["obd2"]["name"] | "", sizeof(obd2.name));
    strlcpy(obd2.mac, doc["obd2"]["mac"] | "", sizeof(obd2.mac));
    obd2.checkPIDSupport = doc["obd2"]["checkPIDSupport"] | false;
    obd2.debug = doc["obd2"]["debug"] | false;
    obd2.specifyNumResponses = doc["obd2"]["specifyNumResponses"] | true;
    obd2.protocol = doc["obd2"]["protocol"] | '0';
}

void OBD2Settings::writeJson(JsonDocument &doc) {
    doc["obd2"]["name"] = obd2.name;
    doc["obd2"]["mac"] = obd2.mac;
    doc["obd2"]["checkPIDSupport"] = obd2.checkPIDSupport;
    doc["obd2"]["debug"] = obd2.debug;
    doc["obd2"]["specifyNumResponses"] = obd2.specifyNumResponses;
    doc["obd2"]["protocol"] = obd2.protocol;
}

String OBD2Settings::getName(const String &alternate) const {
    if (strlen(obd2.name) == 0) {
        return alternate;
    }
    return obd2.name;
}

void OBD2Settings::setName(const char *name) {
    strlcpy(obd2.name, name, sizeof(obd2.name));
}

String OBD2Settings::getMAC() const {
    return obd2.mac;
}

void OBD2Settings::setMAC(const char *mac) {
    strlcpy(obd2.mac, mac, sizeof(obd2.mac));
}

bool OBD2Settings::getCheckPIDSupport() const {
    return obd2.checkPIDSupport;
}

void OBD2Settings::setCheckPIDSupport(bool checkPIDSupport) {
    obd2.checkPIDSupport = checkPIDSupport;
}

bool OBD2Settings::getDebug() const {
    return obd2.debug;
}

void OBD2Settings::setDebug(bool debug) {
    obd2.debug = debug;
}

bool OBD2Settings::getSpecifyNumResponses() const {
    return obd2.specifyNumResponses;
}

void OBD2Settings::setSpecifyNumResponses(bool specifyNumResponses) {
    obd2.specifyNumResponses = specifyNumResponses;
}

char OBD2Settings::getProtocol() const {
    return obd2.protocol;
}

void OBD2Settings::setProtocol(char protocol) {
    obd2.protocol = protocol;
}

SettingsClass Settings;
