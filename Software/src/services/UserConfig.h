#ifndef OSSM_SOFTWARE_USERCONFIG_H
#define OSSM_SOFTWARE_USERCONFIG_H

#include "Arduino.h"

namespace UserConfig {
    enum HomingType {None, Default, SingleSided, DoubleTap};

    /**
     * Built-in defaults for every persisted setting.
     * These are written to NVS on first boot (and after a factory reset);
     * the getters fall back to the same values when a key is missing.
     * Update a default here and it applies everywhere.
     */
    namespace defaults {
        constexpr float speedCurve = 0.8;
        constexpr float sensorLimit = 2.5;
        constexpr const char* deviceName = "CYBORGTOP";
        constexpr bool direction = false;
        constexpr bool reHome = false;
        constexpr int homingType = SingleSided;
        constexpr float railLength = 400.0;
        constexpr float maxAcceleration = 10000.0;
        constexpr float maxRPM = 300.0;
        constexpr float motorSteps = 800.0;
        constexpr float pulleyTeeth = 32.0;
        constexpr float beltPitch = 5.0;
        constexpr float homingSpeed = 25.0;
    }

    void initUserConfig();

    /**
     * Clears the UserConfig NVS namespace. On the next boot
     * initUserConfig() detects the missing initialization flag and
     * re-writes the built-in defaults. The caller is responsible for
     * restarting the device.
     */
    void factoryReset();

    float getSpeedCurve();
    void setSpeedCurve(float value);

    float getSensorLimit();
    void setSensorLimit(float value);

    std::string getDeviceName();
    void setDeviceName(String value);

    bool getDirection();
    void setDirection(bool value);
    void reverseDirection();

    HomingType getHomingType();
    void setHomingType(HomingType value);

    float getRailLength();
    void setRailLength(float value);

    float getMaxAcceleration();
    void setMaxAcceleration(float value);

    bool getReHome();
    void setReHome(bool value);

    float getMotorRPM();
    void setMotorRPM(float value);

    float getMotorStepsPR();
    void setMotorStepsPR(float value);

    float getPulleyTeeth();
    void setPulleyTeeth(float value);

    float getBeltPitch();
    void setBeltPitch(float value);
    
    float getStepsPerMM(float value = 1);
    void setStepsPerMM();

    float getMaxSpeedMMS();
    void setMaxSpeedMMS();

    float getMaxRailLength();
    float getMinRailLength();

    float getHomingSpeed();
    void setHomingSpeed(float value);
}
#endif  // OSSM_SOFTWARE_USERCONFIG_H
