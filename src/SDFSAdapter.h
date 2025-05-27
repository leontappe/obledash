#include <FS.h>
#include <SD.h>

class SD_FS_Wrapper : public fs::FS {
public:
    bool begin(uint8_t csPin = SS, uint32_t speed = SPI_FULL_SPEED) {
        return SD.begin(csPin, speed);
    }

    SDFile open(const char* path, const char* mode = "r") {
        return SD.open(path);
    }
    SDFile open(const String& path, const char* mode = "r") {
        return SD.open((const char*)path.c_str());
    }

    bool exists(const char* path) {
        return SD.exists(path);
    }
    bool exists(const String& path) {
        return SD.exists((const char*)path.c_str());
    }

    bool remove(const char* path) {
        return SD.remove(path);
    }
    bool remove(const String& path) {
        return SD.remove((const char*)path.c_str());
    }

    bool mkdir(const char* path) {
        return SD.mkdir(path);
    }
    bool mkdir(const String& path) {
        return SD.mkdir((const char*)path.c_str());
    }

    bool rmdir(const char* path) {
        return SD.rmdir(path);
    }
    bool rmdir(const String& path) {
        return SD.rmdir((const char*)path.c_str());
    }
};