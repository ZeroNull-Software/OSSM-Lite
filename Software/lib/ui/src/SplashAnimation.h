#ifndef UI_SPLASH_ANIMATION_H
#define UI_SPLASH_ANIMATION_H

namespace ui {
namespace Splash {

    // Boot splash timing. Frames are paced by the hello task at
    // FRAME_DELAY_MS each; measured frame cost on hardware is roughly
    // double that (draw + full I2C refresh). FRAME_COUNT covers the
    // animation plus a tail hold of the finished composition, keeping
    // the splash on screen for ~5 s. Everything is procedural (no
    // bitmaps and no extra fonts), so it costs a little code space
    // and nothing beyond the existing display buffer.
    constexpr int FRAME_COUNT = 76;
    constexpr int FRAME_DELAY_MS = 33;  // ~30 fps target

    // Title layout
    constexpr int TITLE_TOP = 8;

    // Underline growing outward from the center
    constexpr int UNDERLINE_START = 8;
    constexpr int UNDERLINE_FRAMES = 12;

    // Subtitle lines wiping in left-to-right, staggered
    constexpr int SUB1_TOP = 30;
    constexpr int SUB1_START = 16;
    constexpr int SUB2_TOP = 41;
    constexpr int SUB2_START = 22;
    constexpr int SUB_WIPE_FRAMES = 10;

    // Version typewriter
    constexpr int VERSION_START = 30;
    constexpr int VERSION_FRAMES_PER_CHAR = 2;

    // Stylized collab mark between the two title words: an outlined 'X'
    // with a small filled x floating in its hollow center (a low-DPI
    // take on the collaboration mark). X_MARK_SIZE pixels square, kept
    // X_GAP pixels clear of each word, its top row X_MARK_TOP pixels
    // above the title baseline so it overshoots the cap height like a
    // logo mark. Drawn static, with no background animation.
    constexpr int X_MARK_SIZE = 11;
    constexpr int X_GAP = 3;
    constexpr int X_MARK_TOP = 9;

}  // namespace Splash
}  // namespace ui

#endif  // UI_SPLASH_ANIMATION_H
