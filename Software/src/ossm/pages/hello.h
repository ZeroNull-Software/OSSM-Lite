#ifndef OSSM_PAGES_HELLO_H
#define OSSM_PAGES_HELLO_H

namespace pages {

/**
 * Draw the hello/splash screen animation
 * Shows OSSM letters bouncing, then R&D logo, then Kinky Makers logo
 *
 * When the animation completes the remote button is sampled once: if it
 * is held down the device factory-resets (see runFactoryReset),
 * otherwise Done{} is posted so the state machine proceeds to homing.
 */
void drawHello();

/**
 * Performs the factory reset: clears the UserConfig NVS namespace,
 * shows 'Reset ok. Release button to reboot.' on the display, waits for
 * the remote button to be released (so a still-held button cannot
 * immediately re-trigger the reset gesture after the reboot), then
 * restarts the device. Does not return.
 */
[[noreturn]] void runFactoryReset();

}  // namespace pages

#endif  // OSSM_PAGES_HELLO_H
