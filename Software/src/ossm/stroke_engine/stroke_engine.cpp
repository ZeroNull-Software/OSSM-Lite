#include "stroke_engine.h"

#include "components/HeaderBar.h"
#include "ossm/OSSM.h"
#include "ossm/state/ble.h"
#include "ossm/state/calibration.h"
#include "ossm/state/settings.h"
#include "ossm/state/state.h"
#include "services/display.h"
#include "services/stepper.h"
#include "services/tasks.h"
#include "services/UserConfig.h"
#include "Strings.h"
#include "ui.h"

namespace sml = boost::sml;
using namespace sml;

class StrokeEngine Stroker;

namespace stroke_engine {
    static bool isChangeSignificant(float oldPct, float newPct) {
        return oldPct != newPct && (abs(newPct - oldPct) > 0.5 || newPct == 0.0 || newPct == 100.0);
    }

    static float calculateSensation(float sensationPercentage) {
        return float((sensationPercentage * 200.0) / 100.0) - 100.0f;
    }

    static void startStrokeEngineTask(void *pvParameters) {
        SettingPercents lastSetting = settings;
        machineProperties properties{
            .maxSpeed = UserConfig::getMaxSpeedMMS(),
            .maxAcceleration = UserConfig::getMaxAcceleration(),
            .physicalTravel = calibration.measuredStrokeSteps / UserConfig::getStepsPerMM(),
            .stepsPerMillimeter = UserConfig::getStepsPerMM()
        };

        Stroker.begin(&properties, stepper);

        // Translate min/max percentages into StrokeEngine depth+stroke:
        // depth = max position; stroke length = max - min
        Stroker.setDepth(0.01f * settings.maxPosition * abs(properties.physicalTravel), true);
        Stroker.setStroke(0.01f * (settings.maxPosition - settings.minPosition) * abs(properties.physicalTravel), true);
        Stroker.setSensation(calculateSensation(settings.sensation), true);

        auto isInCorrectState = []() {
            // Add any states that you want to support here.
            return stateMachine->is("strokeEngine"_s) ||
                stateMachine->is("strokeEngine.idle"_s) ||
                stateMachine->is("strokeEngine.pattern"_s);
        };

        while (isInCorrectState()) {
            if (isChangeSignificant(lastSetting.speed, settings.speed)) {
                //Curve the speed based on userconfig
                float exp = UserConfig::getSpeedCurve();
                float speed = settings.speed/100.0;
                speed = pow( 1 - pow( 1 - speed, exp), 1 / exp) * 100.0;
                Stroker.setSpeed(speed, true);
                lastSetting.speed = settings.speed;

                // Speed is float, so give a little wiggle room here to assume 0
                if (settings.speed < 0.1f) {
                    Stroker.stopMotion();
                } else if (Stroker.getState() == READY) {
                    Stroker.startPattern();
                }
            }

            if (lastSetting.minPosition != settings.minPosition ||
                lastSetting.maxPosition != settings.maxPosition) {
                float newDepth = 0.01f * settings.maxPosition * abs(properties.physicalTravel);
                float newStroke = 0.01f * (settings.maxPosition - settings.minPosition) * abs(properties.physicalTravel);
                ESP_LOGD("UTILS", "change range: min=%f max=%f depth=%f stroke=%f",
                        settings.minPosition, settings.maxPosition, newDepth, newStroke);
                Stroker.setDepth(newDepth, false);
                Stroker.setStroke(newStroke, true);
                lastSetting.minPosition = settings.minPosition;
                lastSetting.maxPosition = settings.maxPosition;
            }

            if (lastSetting.sensation != settings.sensation) {
                float newSensation = calculateSensation(settings.sensation);
                ESP_LOGD("UTILS", "change sensation: %f %f", settings.sensation,
                        newSensation);
                Stroker.setSensation(newSensation, false);
                lastSetting.sensation = settings.sensation;
            }

            if (lastSetting.pattern != settings.pattern) {
                ESP_LOGD("UTILS", "change pattern: %d", settings.pattern);
                Stroker.setPattern(settings.pattern, false);
                lastSetting.pattern = settings.pattern;
            }
            vTaskDelay(100);
        }
        Stroker.stopMotion();
        vTaskDelete(nullptr);
    }

    void startStrokeEngine() {
        int stackSize = 12 * configMINIMAL_STACK_SIZE;

        xTaskCreatePinnedToCore(startStrokeEngineTask, "startStrokeEngineTask",
                                stackSize, nullptr, configMAX_PRIORITIES - 1,
                                &Tasks::runStrokeEngineTaskH,
                                Tasks::operationTaskCore);

    }

    static void drawStoppingTask(void *pvParameters) {
        showHeaderIcons = true;

        if (isDisplayAvailable() && xSemaphoreTake(displayMutex, 100) == pdTRUE) {
            ui::TextPage page;
            page.title = ui::strings::stopping;
            page.body = ui::strings::stoppingAtMinDepth;
            ui::drawTextPage(display.getU8g2(), page);
            refreshPage(true, true);
            xSemaphoreGive(displayMutex);
        }

        vTaskDelete(nullptr);
    }

    void drawStopping() {
        int stackSize = 3 * configMINIMAL_STACK_SIZE;
        xTaskCreate(drawStoppingTask, "drawStoppingTask", stackSize,
                    nullptr, 1, &Tasks::drawStoppingTaskH);
    }

    static void haltAtMinTask(void *pvParameters) {
        auto isInCorrectState = []() {
            return stateMachine->is("strokeEngine.stopping"_s);
        };

        // Halt any running pattern. No-op if the engine is already READY, and
        // harmless if the exiting stroke task performs the same stop.
        Stroker.stopMotion();

        // Always settle at the currently defined min depth (the min position
        // as a percentage of the measured travel, in steps).
        int32_t minDepthSteps =
            (int32_t)round(0.01f * settings.minPosition * calibration.measuredStrokeSteps);

        if (stepper->getCurrentPosition() != minDepthSteps) {
            ESP_LOGD("UTILS", "halt at min depth: moving to %d steps", minDepthSteps);
            stepper->setAcceleration(
                UserConfig::getStepsPerMM(UserConfig::getMaxAcceleration()));
            // Glide there gently, at ~10% of the max speed
            stepper->setSpeedInHz(
                UserConfig::getStepsPerMM(0.1f * UserConfig::getMaxSpeedMMS()));
            stepper->moveTo(minDepthSteps, false);

            while (stepper->isRunning()) {
                if (!isInCorrectState()) {
                    // Aborted (e.g. emergency stop back to the menu)
                    break;
                }
                vTaskDelay(10);
            }
        }

        if (isInCorrectState()) {
            stateMachine->process_event(Done{});
        }
        vTaskDelete(nullptr);
    }

    void startHaltAtMin() {
        int stackSize = 10 * configMINIMAL_STACK_SIZE;
        xTaskCreatePinnedToCore(haltAtMinTask, "haltAtMinTask", stackSize,
                                nullptr, configMAX_PRIORITIES - 1,
                                &Tasks::runStrokeHaltTaskH,
                                Tasks::operationTaskCore);
    }
}  // namespace stroke_engine