#pragma once

class IO { 
    private:
    int eeAddress = baseAddress;
    int highAddress = eeAddress + 100;
    u_int32_t v;
    u_int32_t lastv;
    u_int16_t storedRPM = 0;
    

    public:
    u_int16_t RPM_SET = 0;
    IO();
    u_int16_t setRPM();
    int16_t count();
    bool ENC_BUTTON();
    bool START_BUTTON();
    void writeToEEPROM(uint8_t what);
    void readEEPROM();
    void encoderInit();

    void ledAction(uint8_t type); //Type: 0 = off, 1 = on, 2 = flashing, 3 = fading, 4 = fast flash


    uint16_t maxRPM = maxRPM_DEFAULT;
    uint16_t minRPM = minRPM_DEFAULT;
    bool viewmode = default_viewmode;
    bool purgeMode = default_purgemode;
    bool buttonPurgeMode = default_buttonpurgemode;
    uint8_t sleep_time_min;
    int16_t currentArray[((absolute_max_rpm-absolute_min_rpm)/rpmScalar)];
};