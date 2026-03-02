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

/// Convert a TurboSpeed value to a short name for logging.
const char *SpeedName(TurboSpeed speed) {
    switch (speed) {
    case TurboSpeed::Slow: return "Slow";
    case TurboSpeed::Medium: return "Medium";
    case TurboSpeed::Fast: return "Fast";
    default: return "Global";
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
            m_counters[bit] = (m_counters[bit] + 1) % GetEffectivePatternLength(bit);
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
    // Counter 0..framesOn-1 = pressed (no change needed, bit already 0 from physical press).
    // Counter framesOn..patternLength-1 = released (set bit to 1).
    for (int bit = 0; bit < 16; ++bit) {
        if (static_cast<uint16_t>(activeTurbo) & (1u << bit)) {
            if (m_counters[bit] >= GetEffectiveFramesOn(bit)) {
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

void TurboEngine::CycleGlobalSpeed() {
    m_globalSpeed = static_cast<TurboSpeed>(
        (static_cast<uint8_t>(m_globalSpeed) + 1) % static_cast<uint8_t>(TurboSpeed::Count));
    const auto &preset = kSpeedPresets[static_cast<uint8_t>(m_globalSpeed)];
    devlog::debug<grp::turbo>("Global turbo speed: {} ({}-on/{}-off)", SpeedName(m_globalSpeed), preset.framesOn,
                              preset.framesOff);
}

bool TurboEngine::CycleButtonSpeed(Button button) {
    if ((button & kTurboEligible) == Button::None) {
        devlog::debug<grp::turbo>("Rejected non-eligible button for speed override");
        return false;
    }

    // Get bit index from button value
    int bit = 0;
    uint16_t val = static_cast<uint16_t>(button);
    while (val > 1) {
        val >>= 1;
        ++bit;
    }

    TurboSpeed current = m_buttonSpeeds[bit];

    if (current == kNoOverride) {
        m_buttonSpeeds[bit] = TurboSpeed::Slow;
    } else if (current == TurboSpeed::Slow) {
        m_buttonSpeeds[bit] = TurboSpeed::Medium;
    } else if (current == TurboSpeed::Medium) {
        m_buttonSpeeds[bit] = TurboSpeed::Fast;
    } else {
        // Fast → revert to global
        m_buttonSpeeds[bit] = kNoOverride;
        m_counters[bit] = 0;
        devlog::debug<grp::turbo>("Button {} speed: Global", ButtonName(button));
        return false;
    }

    // Reset counter for clean speed transition
    m_counters[bit] = 0;
    devlog::debug<grp::turbo>("Button {} speed: {}", ButtonName(button), SpeedName(m_buttonSpeeds[bit]));
    return true;
}

TurboSpeed TurboEngine::GetEffectiveSpeed(Button button) const {
    // Get bit index from button value
    int bit = 0;
    uint16_t val = static_cast<uint16_t>(button);
    while (val > 1) {
        val >>= 1;
        ++bit;
    }

    if (m_buttonSpeeds[bit] != kNoOverride) {
        return m_buttonSpeeds[bit];
    }
    return m_globalSpeed;
}

bool TurboEngine::HasSpeedOverride(Button button) const {
    // Get bit index from button value
    int bit = 0;
    uint16_t val = static_cast<uint16_t>(button);
    while (val > 1) {
        val >>= 1;
        ++bit;
    }

    return m_buttonSpeeds[bit] != kNoOverride;
}

void TurboEngine::SetVideoStandard(ymir::core::config::sys::VideoStandard standard) {
    m_videoStandard = standard;
    devlog::debug<grp::turbo>("Turbo video standard: {}",
                              standard == ymir::core::config::sys::VideoStandard::NTSC ? "NTSC" : "PAL");
}

void TurboEngine::SetSpeedMode(bool active) {
    m_speedMode = active;
    devlog::debug<grp::turbo>("Turbo speed mode: {}", active ? "ON" : "OFF");
}

uint8_t TurboEngine::GetEffectiveFramesOn(int bit) const {
    TurboSpeed speed = m_buttonSpeeds[bit] != kNoOverride ? m_buttonSpeeds[bit] : m_globalSpeed;
    return kSpeedPresets[static_cast<uint8_t>(speed)].framesOn;
}

uint8_t TurboEngine::GetEffectivePatternLength(int bit) const {
    uint8_t framesOn = GetEffectiveFramesOn(bit);
    return framesOn * 2; // Symmetric: framesOff == framesOn
}

void TurboEngine::Reset() {
    m_turboEnabled = Button::None;
    m_counters.fill(0);
    m_prevPhysical = Button::Default;
    m_toggleMode = false;
    m_globalSpeed = TurboSpeed::Medium;
    m_buttonSpeeds = MakeDefaultButtonSpeeds();
    m_speedMode = false;
    devlog::debug<grp::turbo>("Turbo state reset");
}

} // namespace app::input
