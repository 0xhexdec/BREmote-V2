// OTA
#include <Update.h>

void performUpdate(Stream &updateSource, size_t updateSize) {
  if (Update.begin(updateSize)) {
    size_t written = Update.writeStream(updateSource);
    if (written == updateSize) {
      Serial.println("Written : " + String(written) + " successfully");
    } else {
      Serial.println("Written only : " + String(written) + "/" + String(updateSize) + ". Retry?");
    }
    if (Update.end()) {
      Serial.println("OTA done!");
      if (Update.isFinished()) {
        Serial.println("Update successfully completed. Rebooting.");
      } else {
        Serial.println("Update not finished? Something went wrong!");
      }
    } else {
      Serial.println("Error Occurred. Error #: " + String(Update.getError()));
    }

  } else {
    Serial.println("Not enough space to begin OTA");
  }
}

bool startOtaUpdate(size_t updateSize) {
  return Update.begin(updateSize);
}

bool writeData(uint8_t *data, size_t len) {
  size_t bytesWritten = Update.write(data, len);
  return (bytesWritten == len);
}

void abort() {
  Update.abort();
}

bool endOtaUpdate() {
    if (Update.end()) {
      Serial.println("OTA done!");
      if (Update.isFinished()) {
        Serial.println("Update successfully completed. Rebooting.");
        return true;
      } else {
        Serial.println("Something went wrong with ota update!");
      }
    } else {
      Serial.println("Error Occurred. Error #: " + String(Update.getError()));
    }
    return false;
}
