// EEPROM format marker and factory reset implementation

#include <EEPROM.h>

#define FORMAT_MARKER 0xABCD
#define FACTORY_RESET_MARKER 0x0000

void checkEEPROM() {
    int storedMarker = EEPROM.read(0);
    if (storedMarker != FORMAT_MARKER) {
        factoryReset();
    }
}

void factoryReset() {
    EEPROM.write(0, FACTORY_RESET_MARKER);
    // Clear other settings to default values
    EEPROM.write(1, 0); // Example of resetting a setting
}