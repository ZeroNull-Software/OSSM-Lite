#ifndef OSSM_USER_CALIBRATION_CALIBRATION_H
#define OSSM_USER_CALIBRATION_CALIBRATION_H

namespace user_calibration {

/**
 * Draw and run the user calibration sequence.
 *
 * The machine first moves to the current min position, then lets the user jog
 * the min and max stroke positions with the knob at a fixed, safe calibration
 * speed. Each position is confirmed with a click. A long-press cancels. The
 * state machine is responsible for entering this state and for leaving it
 * (via the Done event emitted here on completion, or long-press to cancel).
 */
void drawUserCalibration();

/**
 * Confirm handler for the knob click during calibration.
 *
 * Stores the current position as the min (then the max) stroke position and
 * advances the calibration sequence. No-op outside the set-min / set-max
 * phases.
 */
void onConfirm();

/**
 * Cancel handler for the long-press during calibration.
 *
 * Safe to call from the button task: it only flags the calibration task and
 * restores a coherent min/max range. The task itself brings the motor to rest
 * and hands the stepper back to the stroke engine, so the motor is never
 * driven by two owners at once.
 */
void onCancel();

}  // namespace user_calibration

#endif  // OSSM_USER_CALIBRATION_CALIBRATION_H
