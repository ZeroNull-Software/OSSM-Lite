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

    // The last target position commanded to the stepper while jogging
    // SetMin/SetMax.
    int32_t jogTargetSteps = 0;

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

    // Redraw the jog prompt showing the current target position.
    void drawJogPrompt(Phase activePhase) {
        if (!isDisplayAvailable()) {
            return;
        }
        char valueText[16];
        snprintf(valueText, sizeof(valueText), "%s: %d%%",
                 activePhase == Phase::SetMin ? "Min" : "Max",
                 (int)stepsToPct(jogTargetSteps));
        if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
            ui::TextPage page;
            page.title = "Calibration";
            page.subtitle = valueText;
            page.body = activePhase == Phase::SetMin
                            ? "Set the min depth and click"
                            : "Set the max depth and click";
            page.bottomText = "Long press to cancel";
            page.centerBody = true;
            ui::drawTextPage(display.getU8g2(), page);
            refreshPage(true, true);
            xSemaphoreGive(displayMutex);
        }
    }

    // Jog: move the machine to the knob's position one detent at a time, at
    // the calibration speed. Each accepted detent commands a single
    // point-to-point move that decelerates to a stop and holds, so the knob
    // value is the commanded point rather than a continuously followed
    // position. While a move is in flight, further detents are not applied
    // until the move completes (one move per detent, in order), so noisy
    // encoder edges can never re-target a running move and stall or reverse
    // the motor mid-travel. When clampToMin is set the target never goes
    // below the stored min position.
    void jogUntilConfirm(Phase activePhase, bool clampToMin) {
        int32_t minSteps = clampToMin ? pctToSteps(settings.minPosition) : 0;
        int32_t maxSteps = (int32_t)calibration.measuredStrokeSteps;

        // Start from the knob's current value and the machine's current
        // position so the first move is triggered by an actual detent.
        int lastAccepted = (int)encoder.readEncoder();
        jogTargetSteps = stepper->getCurrentPosition();
        drawJogPrompt(activePhase);

        while (stateMachine->is("usercalibration"_s) && phase == activePhase) {
            int current = (int)encoder.readEncoder();
            // Only accept a new target while at rest; step the accepted value
            // one detent at a time so every detent gets its own move.
            if (current != lastAccepted && !stepper->isRunning()) {
                lastAccepted += (current > lastAccepted) ? 1 : -1;
                int32_t target = constrain(pctToSteps(lastAccepted), minSteps, maxSteps);
                if (target != jogTargetSteps) {
                    jogTargetSteps = target;
                    applyCalibrationProfile();
                    stepper->moveTo(target, false);
                    drawJogPrompt(activePhase);
                }
            }
            vTaskDelay(20);
        }
    }

}  // namespace

void onConfirm() {
    // Store the commanded target (jogTargetSteps) rather than the transient
    // position. If the user clicks while the motor is still moving toward
    // the last detent, the machine will arrive at exactly that position, so
    // the stored value is deterministic and matches the knob's reading.
    switch (phase) {
        case Phase::SetMin:
            settings.minPosition =
                constrain(stepsToPct(jogTargetSteps), 0.0f, 99.0f);
            phase = Phase::SetMax;
            break;
        case Phase::SetMax: {
            float maxPct = stepsToPct(jogTargetSteps);
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
            jogUntilConfirm(Phase::SetMin, /*clampToMin=*/false);
            if (!stateMachine->is("usercalibration"_s)) {
                stepper->stopMove();
                vTaskDelete(nullptr);
            }

            // 3. Set the max depth (never below the new min).
            phase = Phase::SetMax;
            encoder.setEncoderValue((int)round(settings.maxPosition));
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
