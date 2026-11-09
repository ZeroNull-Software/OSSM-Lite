#include "UserConfig.h"

#include <Preferences.h>

namespace UserConfig {
    Preferences userConfig;
    std::string deviceName = "";
    float railLength = -1;
    float maxAcceleration = -1;
    float maxRPM = -1;
    float motorStepPerRevolution = -1;
    float pulleyToothCount = -1;
    float beltPitchMm = -1;
    float stepsPerMM = -1;
    float maxSpeedMMS = -1;
    float homingSpeed = -1;
    float sensorLimit = -1;
    float speedCurve = -1;

    const char* pref = "UserConfig";
    const char* tag = "USER CONFIG";
    // NOTE: NVS keys are limited to 15 characters, hence the abbreviation.
    const char* prefsInitializedKey = "is_prefs_init";

    /**
     * Writes every built-in default (UserConfig::defaults) to the
     * preferences namespace. The handle must already be open read/write.
     * Keys must stay in sync with the getters/setters below.
     */
    static void writeDefaultConfig() {
        userConfig.putFloat("SpeedCurve", defaults::speedCurve);
        userConfig.putFloat("SensorLimit", defaults::sensorLimit);
        userConfig.putString("DeviceName", defaults::deviceName);
        userConfig.putBool("Direction", defaults::direction);
        userConfig.putBool("ReHome", defaults::reHome);
        userConfig.putInt("HomingType", defaults::homingType);
        userConfig.putFloat("RailLength", defaults::railLength);
        userConfig.putFloat("MaxAcceleration", defaults::maxAcceleration);
        userConfig.putFloat("MaxRPM", defaults::maxRPM);
        userConfig.putFloat("MotorSteps", defaults::motorSteps);
        userConfig.putFloat("PulleyTeeth", defaults::pulleyTeeth);
        userConfig.putFloat("BeltPitch", defaults::beltPitch);
        userConfig.putFloat("HomingSpeed", defaults::homingSpeed);
        ESP_LOGI(tag, "Default configuration written to NVS");
    }

    void initUserConfig() {
        userConfig.begin(pref);
        ESP_LOGI(tag, "Initializing Preferences");

        // First boot (or after a factory reset): the flag is missing, so
        // persist the defaults before anything reads them back.
        if (!userConfig.getBool(prefsInitializedKey, false)) {
            ESP_LOGI(tag, "Preferences not initialized, writing defaults");
            writeDefaultConfig();
            userConfig.putBool(prefsInitializedKey, true);
        }

        userConfig.end();
    }

    void factoryReset() {
        userConfig.begin(pref, false);
        userConfig.clear();
        userConfig.end();
        ESP_LOGW(tag, "Factory reset: UserConfig NVS namespace cleared");
    }

    float readNVSFloat(const char* key, float def) {
        userConfig.begin(pref, true);
        float value = userConfig.getFloat(key, def);
        userConfig.end();
        ESP_LOGI(tag,"%s read: %f", key, value);
        return value;
    }
    void writeNVSFloat(const char* key, float value) {
        userConfig.begin(pref, false);
        userConfig.putFloat(key, value);
        userConfig.end();
        ESP_LOGI(tag, "%s write: %f", key, value);
    }

    bool readNVSBool(const char* key, bool def) {
        userConfig.begin(pref, true);
        bool value = userConfig.getBool(key, def);
        userConfig.end();
        ESP_LOGI(tag,"%s read: %d", key, value);
        return value;
    }
    void writeNVSBool(const char* key, bool value) {
        userConfig.begin(pref, false);
        userConfig.putBool(key, value);
        userConfig.end();
        ESP_LOGI(tag,"%s write: %d", key, value);
    }

    float getSpeedCurve() {
        if (speedCurve < 0) {
            speedCurve = readNVSFloat("SpeedCurve", defaults::speedCurve);
        }
        return speedCurve;
    }
    void setSpeedCurve(float value) {
        value = constrain(value, 0.5, 1.0);
        writeNVSFloat("SpeedCurve", value);
        speedCurve = value;
    }

    float getSensorLimit() {
        if (sensorLimit < 0) {
            sensorLimit = readNVSFloat("SensorLimit", defaults::sensorLimit);
        }
        return sensorLimit;
    }
    void setSensorLimit(float value){
        value = constrain(value, 1.5, 5.0);
        writeNVSFloat("SensorLimit", value);
        sensorLimit = value;
    }

    std::string getDeviceName() {
        if (deviceName.empty()) {
            userConfig.begin(pref, true);
            deviceName = userConfig.getString("DeviceName", defaults::deviceName).c_str();
            userConfig.end();
            ESP_LOGD("USER CONFIG", "Name read: %s", deviceName.c_str());
        }
        return deviceName;
    }
    void setDeviceName(String value) {
        userConfig.begin(pref, false);
        userConfig.putString("DeviceName", value);
        userConfig.end();
        ESP_LOGI(tag, "Rename write: %s", value.c_str());
        ESP.restart();
    }

    bool getDirection() {
        return readNVSBool("Direction", defaults::direction);
    }
    void setDirection(bool value) {
        writeNVSBool("Direction", value);
        ESP.restart();
    }
    void reverseDirection() {
        setDirection(!getDirection());
    }

    bool getReHome() {
        return readNVSBool("ReHome", defaults::reHome);
    }
    void setReHome(bool value) {
        writeNVSBool("ReHome", value);
    }

    HomingType getHomingType() {
        userConfig.begin(pref, true);
        HomingType output = static_cast<HomingType>(userConfig.getInt("HomingType", defaults::homingType));
        userConfig.end();
        ESP_LOGI(tag,"Homing type read: %d", output);
        return output;
    }
    void setHomingType(HomingType value) {
        userConfig.begin(pref, false);
        userConfig.putInt("HomingType", value);
        userConfig.end();
        ESP_LOGI(tag, "Homing type write: %d", value);
    }

    float getRailLength() {
        if (railLength < 0) {
            railLength = readNVSFloat("RailLength", defaults::railLength);
        }
        return railLength;
    }
    void setRailLength(float value) {
        value = constrain(value, getMinRailLength(), getMaxRailLength());
        writeNVSFloat("RailLength", value);
        railLength = value;
    }

    float getMaxAcceleration() {
        if (maxAcceleration < 0) {
            maxAcceleration = readNVSFloat("MaxAcceleration", defaults::maxAcceleration);
        }
        return maxAcceleration;
    }
    void setMaxAcceleration(float value) {
        writeNVSFloat("MaxAcceleration", value);
        maxAcceleration = value;
    }

    float getMotorRPM() {
        if (maxRPM < 0) {
            maxRPM = readNVSFloat("MaxRPM", defaults::maxRPM);
        }
        return maxRPM;
    }
    void setMotorRPM(float value) {
        writeNVSFloat("MaxRPM", value);
        maxRPM = value;
        setMaxSpeedMMS();
    }

    float getMotorStepsPR() {
        if (motorStepPerRevolution < 0) {
            motorStepPerRevolution = readNVSFloat("MotorSteps", defaults::motorSteps);
        }
        return motorStepPerRevolution;
    }
    void setMotorStepsPR(float value) {
        writeNVSFloat("MotorSteps", value);
        motorStepPerRevolution = value;
        setStepsPerMM();
    }

    float getPulleyTeeth(){
        if (pulleyToothCount < 0) {
            pulleyToothCount = readNVSFloat("PulleyTeeth", defaults::pulleyTeeth);
        }
        return pulleyToothCount;
    }
    void setPulleyTeeth(float value){
        writeNVSFloat("PulleyTeeth", value);
        pulleyToothCount = value;
        setMaxSpeedMMS();
        setStepsPerMM();
    }

    float getBeltPitch(){
        if (beltPitchMm < 0) {
            beltPitchMm = readNVSFloat("BeltPitch", defaults::beltPitch);
        }
        return beltPitchMm;
    }
    void setBeltPitch(float value){
        writeNVSFloat("BeltPitch", value);
        beltPitchMm = value;
        setMaxSpeedMMS();
        setStepsPerMM();
    }

    float getStepsPerMM(float value){
        if (stepsPerMM < 0) {
            setStepsPerMM();
        }
        return stepsPerMM * value;
    }
    void setStepsPerMM() {
        //cache this
        stepsPerMM = (getMotorStepsPR() / (getPulleyTeeth() * getBeltPitch()));
    }

    float getMaxSpeedMMS() {
        if (maxSpeedMMS < 0) {
            setMaxSpeedMMS();
        }
        return maxSpeedMMS;
    }

    void setMaxSpeedMMS(){
        //cache this
        maxSpeedMMS = getMotorRPM() / 60.0 * getPulleyTeeth() * getBeltPitch();
    }

    float getMaxRailLength(){
        float rail = getRailLength();
        if (rail > 500.0) {
            return rail;
        }
        return 500.0;
    }

    float getMinRailLength() {
        return 50.0;
    }

    float getHomingSpeed(){
        if (homingSpeed < 0) {
            homingSpeed = readNVSFloat("HomingSpeed", defaults::homingSpeed);
        }
        return homingSpeed;
    }
    void setHomingSpeed(float value){
        value = constrain(value,10,40);
        writeNVSFloat("HomingSpeed", value);
        homingSpeed = value;
    }
}
