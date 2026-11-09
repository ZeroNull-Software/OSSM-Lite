#include "hello.h"

#include "HelloAnimation.h"
#include "components/HeaderBar.h"
#include "constants/Pins.h"
#include "constants/Version.h"
#include "ossm/Events.h"
#include "ossm/state/state.h"
#include "services/display.h"
#include "services/tasks.h"
#include "services/UserConfig.h"
#include "ui.h"

namespace pages {

    /**
     * Debounced sample of the remote encoder switch, taken right after the
     * splash animation completes. Holding the button at that moment is the
     * factory-reset gesture. A majority vote over a few samples keeps
     * contact bounce from wiping the configuration.
     */
    static bool isRemoteButtonHeld() {
        constexpr int sampleCount = 5;
        int pressedSamples = 0;

        for (int i = 0; i < sampleCount; i++) {
            if (digitalRead(Pins::Remote::encoderSwitch) == HIGH) {
                pressedSamples++;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        return pressedSamples * 2 > sampleCount;
    }

    [[noreturn]] void runFactoryReset() {
        ESP_LOGW("Hello", "Factory reset requested, clearing UserConfig NVS");
        UserConfig::factoryReset();

        if (isDisplayAvailable() && xSemaphoreTake(displayMutex, 100) == pdTRUE) {
            ui::clearPage(display.getU8g2(), true, true);
            ui::drawWrappedText(display.getU8g2(), 0, 20,
                                ui::strings::resetOkReleaseToReboot, true);
            refreshPage(true, true);
            xSemaphoreGive(displayMutex);
        }

        // Wait for the button to be released before rebooting, so a
        // still-held button cannot immediately re-trigger the reset
        // gesture on the next boot (accidental reset loop).
        while (digitalRead(Pins::Remote::encoderSwitch) == HIGH) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        // Let the contact settle after release.
        vTaskDelay(pdMS_TO_TICKS(100));

        ESP.restart();
    }

    static void drawHelloTask(void *pvParameters) {
        showHeaderIcons = false;

        if (isDisplayAvailable()) {
            for (int i = 0; i < ui::HELLO_FRAME_COUNT; i++) {
                if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
                    ui::drawHelloFrame(display.getU8g2(), ui::HELLO_FRAMES[i], VERSION);
                    refreshPage(true, true);
                    xSemaphoreGive(displayMutex);
                }
                vTaskDelay(1);
            }
        }

        // Factory-reset check: if the remote button is held when the
        // animation completes, wipe the UserConfig NVS namespace and
        // restart. The defaults are re-written on the next boot because
        // the initialization flag was cleared along with everything else.
        if (isRemoteButtonHeld()) {
            runFactoryReset();
        }

        stateMachine->process_event(Done{});
        vTaskDelete(nullptr);
    }

    void drawHello() {
        int stackSize = 3 * configMINIMAL_STACK_SIZE;
        xTaskCreate(drawHelloTask, "drawHello", stackSize, nullptr, 1,
                    &Tasks::drawHelloTaskH);
    }

}  // namespace pages
