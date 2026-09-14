#include "calibration.h"

#include <algorithm>

#include "components/HeaderBar.h"
#include "ossm/Events.h"
#include "ossm/state/calibration.h"
#include "ossm/state/settings.h"
#include "ossm/state/state.h"
#include "services/display.h"
#include "services/encoder.h"
#include "services/stepper.h"
#include "services/tasks.h"
#include "services/UserConfig.h"
#include "ui.h"

namespace sml = boost::sml;
using namespace sml;

namespace user_calibration {
namespace {

    // The stage of the calibration sequence. `onConfirm()` (driven by the knob
    // click) advances SetMin -> SetMax -> ReturnMin; the task reacts to it.
    enum class Phase { Starting, SetMin, SetMax, ReturnMin, Done };

    Phase phase = Phase::Starting;

    // Calibration motion limits, in mm (converted to steps at task start).
    constexpr float kCalibrationSpeedPercent = 10.0f;  // 10% of max speed
    constexpr float kCalibrationSpeedCapMMS = 50.0f;   // ... capped at 50 mm/s
    constexpr float kCalibrationAccelMMS2 = 2000.0f;   // 2000 mm/s^2

    int32_t calibrationSpeedSteps = 0;
    int32_t calibrationAccelSteps = 0;

    int32_t pctToSteps(float pct) {
        return (int32_t)round(0.01f * pct * (float)calibration.measuredStrokeSteps);
    }

    float stepsToPct(int32_t steps) {
        if (calibration.measuredStrokeSteps <= 0) {
            return 0.0f;
        }
        return float(steps) / float(calibration.measuredStrokeSteps) * 100.0f;
    }

    // The motor is controlled directly here (the Stroke Engine task has already
    // self-terminated while in this state), so re-assert the safe profile on
    // every (re)start of a move.
    void applyCalibrationProfile() {
        stepper->enableOutputs();
        stepper->setDirectionPin(Pins::Driver::motorDirectionPin, UserConfig::getDirection());
        stepper->setAcceleration(calibrationAccelSteps);
        stepper->setSpeedInHz(calibrationSpeedSteps);
    }

    // Wait for the motor to come to rest, giving up if we leave the state.
    bool waitUntilStopped() {
        while (stepper->isRunning()) {
            if (!stateMachine->is("usercalibration"_s)) {
                return false;
            }
            vTaskDelay(10);
        }
        return true;
    }

    bool drawPrompt(const char *title, const char *body, const char *bottom) {
        if (!isDisplayAvailable()) {
            return false;
        }
        if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
            ui::TextPage page;
            page.title = title;
            page.body = body;
            page.bottomText = bottom;
            page.centerBody = true;
            ui::drawTextPage(display.getU8g2(), page);
            refreshPage(true, true);
            xSemaphoreGive(displayMutex);
            return true;
        }
        return false;
    }

    // Jog: follow the knob at calibration speed until `onConfirm()` advances the
    // phase or the state changes. When clampToMin is set the target never goes
    // below the stored min position.
    void jogUntilConfirm(Phase activePhase, bool clampToMin) {
        int32_t minSteps = clampToMin ? pctToSteps(settings.minPosition) : 0;
        int32_t maxSteps = (int32_t)calibration.measuredStrokeSteps;
        int32_t lastTarget = stepper->getCurrentPosition();

        while (stateMachine->is("usercalibration"_s) && phase == activePhase) {
            int32_t target = pctToSteps(encoder.readEncoder());
            target = constrain(target, minSteps, maxSteps);
            if (target != lastTarget) {
                lastTarget = target;
                applyCalibrationProfile();
                stepper->moveTo(target, false);
            }
            vTaskDelay(20);
        }
    }

}  // namespace

void onConfirm() {
    switch (phase) {
        case Phase::SetMin:
            settings.minPosition =
                constrain(stepsToPct(stepper->getCurrentPosition()), 0.0f, 99.0f);
            phase = Phase::SetMax;
            break;
        case Phase::SetMax: {
            float maxPct = stepsToPct(stepper->getCurrentPosition());
            settings.maxPosition = constrain(maxPct, settings.minPosition, 100.0f);
            phase = Phase::ReturnMin;
            break;
        }
        default:
            break;
    }
}

void drawUserCalibration() {
    int stackSize = 10 * configMINIMAL_STACK_SIZE;
    xTaskCreatePinnedToCore(
        [](void *pvParameters) {
            float calibrationSpeedMMS = std::min(
                kCalibrationSpeedPercent / 100.0f * UserConfig::getMaxSpeedMMS(),
                kCalibrationSpeedCapMMS);
            calibrationSpeedSteps = (int32_t)UserConfig::getStepsPerMM(calibrationSpeedMMS);
            calibrationAccelSteps = (int32_t)UserConfig::getStepsPerMM(kCalibrationAccelMMS2);
            if (calibrationSpeedSteps < 1) {
                calibrationSpeedSteps = 1;
            }

            encoder.setBoundaries(0, 100, false);
            encoder.setAcceleration(10);
            showHeaderIcons = true;

            // 1. Move to the current min position at calibration speed.
            phase = Phase::Starting;
            applyCalibrationProfile();
            drawPrompt("Calibration", "Starting Calibration...", nullptr);
            stepper->moveTo(pctToSteps(settings.minPosition), false);
            if (!waitUntilStopped()) {
                stepper->stopMove();
                vTaskDelete(nullptr);
            }

            // 2. Set the min depth.
            phase = Phase::SetMin;
            encoder.setEncoderValue((int)round(settings.minPosition));
            drawPrompt("Calibration", "Set the min depth and click", "Long press to cancel");
            jogUntilConfirm(Phase::SetMin, /*clampToMin=*/false);
            if (!stateMachine->is("usercalibration"_s)) {
                stepper->stopMove();
                vTaskDelete(nullptr);
            }

            // 3. Set the max depth (never below the new min).
            phase = Phase::SetMax;
            encoder.setEncoderValue((int)round(settings.maxPosition));
            drawPrompt("Calibration", "Set the max depth and click", "Long press to cancel");
            jogUntilConfirm(Phase::SetMax, /*clampToMin=*/true);
            if (!stateMachine->is("usercalibration"_s)) {
                stepper->stopMove();
                vTaskDelete(nullptr);
            }

            // 4. Ease back to the min position so the next pattern starts there.
            phase = Phase::ReturnMin;
            applyCalibrationProfile();
            drawPrompt("Calibration", "Returning to min position...", nullptr);
            stepper->moveTo(pctToSteps(settings.minPosition), false);
            if (waitUntilStopped()) {
                phase = Phase::Done;
                stateMachine->process_event(Done{});
            }

            vTaskDelete(nullptr);
        },
        "userCalibrationTask", stackSize, nullptr, configMAX_PRIORITIES - 1,
        &Tasks::runUserCalibrationTaskH, Tasks::operationTaskCore);
}

}  // namespace user_calibration
