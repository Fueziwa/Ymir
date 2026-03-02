#pragma once

#include <ymir/hw/smpc/peripheral/peripheral_state_common.hpp>
#include <ymir/util/dev_log.hpp>

#include <array>
#include <cstdint>

namespace app::input {

namespace grp {

    struct turbo {
        static constexpr bool enabled = true;
        static constexpr devlog::Level level = devlog::level::debug;
        static constexpr std::string_view name = "Turbo";
    };

} // namespace grp

/// Turbo-eligible buttons — D-pad directions are excluded.
inline constexpr auto kTurboEligible = ymir::peripheral::Button::A | ymir::peripheral::Button::B |
                                       ymir::peripheral::Button::C | ymir::peripheral::Button::X |
                                       ymir::peripheral::Button::Y | ymir::peripheral::Button::Z |
                                       ymir::peripheral::Button::L | ymir::peripheral::Button::R |
                                       ymir::peripheral::Button::Start;

/// Per-button turbo engine with frame-synchronized hold-to-fire pattern.
///
/// Active-low convention: bit 0 = pressed, bit 1 = released.
/// Apply() returns a modified copy of button state — never mutates the input.
class TurboEngine {
public:
    /// Turbo pattern constants: 2 frames on, 2 frames off (~15 Hz at 60 fps).
    static constexpr uint8_t kFramesOn = 2;
    static constexpr uint8_t kFramesOff = 2;
    static constexpr uint8_t kPatternLength = kFramesOn + kFramesOff;

    /// Toggle turbo for a specific button.
    /// Returns true if turbo is now enabled for that button, false if disabled or rejected.
    bool ToggleTurbo(ymir::peripheral::Button button);

    /// Check if turbo is enabled for a specific button.
    bool IsTurboEnabled(ymir::peripheral::Button button) const;

    /// Get the full turbo-enabled bitmask.
    ymir::peripheral::Button GetTurboMask() const { return m_turboEnabled; }

    /// Advance frame counters. Call exactly once per emulated frame.
    void AdvanceFrame();

    /// Apply turbo pattern to physical button state.
    /// @param physicalButtons Raw input from SharedContext (active-low: 0=pressed, 1=released).
    /// @return Modified button state for SMPC consumption.
    ymir::peripheral::Button Apply(ymir::peripheral::Button physicalButtons);

    /// Check if turbo toggle mode is active.
    bool IsToggleModeActive() const { return m_toggleMode; }

    /// Set turbo toggle mode on or off.
    void SetToggleMode(bool active);

    /// Reset all turbo state (session end).
    void Reset();

private:
    /// Which buttons have turbo enabled (Button bitmask).
    ymir::peripheral::Button m_turboEnabled = ymir::peripheral::Button::None;

    /// Per-button frame counters indexed by bit position (0–15).
    std::array<uint8_t, 16> m_counters{};

    /// Previous frame's physical button state (for detecting new presses).
    ymir::peripheral::Button m_prevPhysical = ymir::peripheral::Button::Default; // All released

    /// Toggle mode active flag.
    bool m_toggleMode = false;
};

} // namespace app::input
