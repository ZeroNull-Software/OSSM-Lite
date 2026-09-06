#ifndef OSSM_STROKE_ENGINE_STROKE_ENGINE_H
#define OSSM_STROKE_ENGINE_STROKE_ENGINE_H

namespace stroke_engine {

    /**
     * Start the stroke engine motion task
     * Uses StrokeEngine library for complex motion patterns
     */
    void startStrokeEngine();

    /**
     * Draw the page shown while the machine halts at the min depth
     */
    void drawStopping();

    /**
     * Start the task that smoothly halts the machine at the currently
     * defined min depth, then fires Done to continue the state machine
     */
    void startHaltAtMin();

}  // namespace stroke_engine

#endif  // OSSM_STROKE_ENGINE_STROKE_ENGINE_H
