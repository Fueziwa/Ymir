#include "turbo_engine.hpp"

namespace app::input {

using Button = ymir::peripheral::Button;

namespace {

/// Convert a single Button enum value to a short name for logging.
const char *ButtonName(Button button) {
    switch (button) {
    case Button::A: return "A";
    case Button::B: return "B";
    case Button::C: return "C";
    case Button::X: return "X";
    case Button::Y: return "Y";
    case Button::Z: return "Z";
    case Button::L: return "L";
    case Button::R: return "R";
    case Button::Start: return "Start";
    default: return "?";
    }
}

} // anonymous namespace

bool TurboEngine::ToggleTurbo(Button button) {
    if ((button & kTurboEligible) == Button::None) {
        devlog::debug<grp::turbo>("Rejected non-eligible button for turbo");
        return false;
    }
    m_turboEnabled ^= button;
    bool nowEnabled = (m_turboEnabled & button) != Button::None;
    devlog::debug<grp::turbo>("Turbo {}: {}", ButtonName(button), nowEnabled ? "ON" : "OFF");
    return nowEnabled;
}

bool TurboEngine::IsTurboEnabled(Button button) const {
    return (m_turboEnabled & button) != Button::None;
}

void TurboEngine::AdvanceFrame() {
    for (int bit = 0; bit < 16; ++bit) {
        uint16_t mask = 1u << bit;
        // Button is turbo-enabled and was physically pressed last frame (active-low: bit 0 = pressed)
        bool turboActive = (static_cast<uint16_t>(m_turboEnabled) & mask) != 0;
        bool wasPressed = (static_cast<uint16_t>(m_prevPhysical) & mask) == 0;
        if (turboActive && wasPressed) {
            m_counters[bit] = (m_counters[bit] + 1) % kPatternLength;
        }
    }
}

Button TurboEngine::Apply(Button physicalButtons) {
    // Start with physical state
    Button result = physicalButtons;

    // Identify buttons that are physically pressed and have turbo enabled.
    // Active-low: ~physicalButtons gives us the "pressed" mask.
    Button activeTurbo = static_cast<Button>(
        static_cast<uint16_t>(~physicalButtons) &
        static_cast<uint16_t>(m_turboEnabled));

    // Detect newly pressed buttons (were released last frame, now pressed).
    // Active-low: m_prevPhysical bit=1 means was released, ~physicalButtons bit=1 means now pressed.
    Button newlyPressed = static_cast<Button>(
        static_cast<uint16_t>(m_prevPhysical) &
        static_cast<uint16_t>(~physicalButtons));

    // Reset counters for newly pressed turbo buttons.
    Button newTurboPress = newlyPressed & m_turboEnabled;
    if (newTurboPress != Button::None) {
        for (int bit = 0; bit < 16; ++bit) {
            if (static_cast<uint16_t>(newTurboPress) & (1u << bit)) {
                m_counters[bit] = 0;
            }
        }
    }

    // Apply turbo pattern to each active turbo button.
    // Counter 0..kFramesOn-1 = pressed (no change needed, bit already 0 from physical press).
    // Counter kFramesOn..kPatternLength-1 = released (set bit to 1).
    for (int bit = 0; bit < 16; ++bit) {
        if (static_cast<uint16_t>(activeTurbo) & (1u << bit)) {
            if (m_counters[bit] >= kFramesOn) {
                // Turbo says "release" — set the bit (active-low: 1 = released)
                result |= static_cast<Button>(1u << bit);
            }
            // else: turbo says "press", bit already 0 from physical press, no change needed
        }
    }

    m_prevPhysical = physicalButtons;
    return result;
}

void TurboEngine::SetToggleMode(bool active) {
    m_toggleMode = active;
    devlog::debug<grp::turbo>("Turbo toggle mode: {}", active ? "ON" : "OFF");
}

void TurboEngine::Reset() {
    m_turboEnabled = Button::None;
    m_counters.fill(0);
    m_prevPhysical = Button::Default;
    m_toggleMode = false;
    devlog::debug<grp::turbo>("Turbo state reset");
}

} // namespace app::input
