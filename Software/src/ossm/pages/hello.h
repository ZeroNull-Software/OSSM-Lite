#ifndef OSSM_PAGES_HELLO_H
#define OSSM_PAGES_HELLO_H

namespace pages {

/**
 * Draw the boot splash screen: "CYBORGTOP x OSSM" branding with the
 * "Based on the OSSM Lite Firmware" credit and the firmware version.
 * The splash is a short procedural animation (growing underline,
 * subtitle wipes, version typewriter) rendered frame by frame - see
 * ui::drawSplashFrame and SplashAnimation.h.
 *
 * When the animation completes the remote button is sampled once: if it
 * is held down the device factory-resets (see runFactoryReset),
 * otherwise Done{} is posted so the state machine proceeds to homing.
 * A long press during the splash is handled by the state machine's
 * hello + longPress transition; the splash task stops drawing as soon
 * as the machine leaves the hello state.
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
