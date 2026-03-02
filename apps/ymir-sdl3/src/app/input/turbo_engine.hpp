#pragma once

#include <ymir/core/configuration_defs.hpp>
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

/// Turbo speed presets — 3 levels cycling in a wrapping loop.
enum class TurboSpeed : uint8_t { Slow, Medium, Fast, Count };

/// Frame counts for each preset. Symmetric: framesOn == framesOff.
struct TurboSpeedPreset {
    uint8_t framesOn;
    uint8_t framesOff;
    float hzNTSC; // Approximate Hz at 60fps
    float hzPAL;  // Approximate Hz at 50fps
};

/// Preset table: slow (3on/3off ~10Hz), medium (2on/2off ~15Hz), fast (1on/1off ~30Hz).
inline constexpr TurboSpeedPreset kSpeedPresets[] = {
    {3, 3, 10.0f, 8.33f}, // Slow
    {2, 2, 15.0f, 12.5f}, // Medium
    {1, 1, 30.0f, 25.0f}, // Fast
};

/// Per-button turbo engine with frame-synchronized hold-to-fire pattern.
///
/// Active-low convention: bit 0 = pressed, bit 1 = released.
/// Apply() returns a modified copy of button state — never mutates the input.
class TurboEngine {
public:
    /// Sentinel value: button uses global speed (no per-button override).
    static constexpr auto kNoOverride = static_cast<TurboSpeed>(0xFF);

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

    /// Cycle the global speed preset: slow → medium → fast → slow.
    void CycleGlobalSpeed();

    /// Get the current global speed preset.
    TurboSpeed GetGlobalSpeed() const { return m_globalSpeed; }

    /// Cycle a button's per-button speed override: slow → medium → fast → [revert to global] → slow.
    /// Returns true if an override is now set, false if reverted to global.
    bool CycleButtonSpeed(ymir::peripheral::Button button);

    /// Get a button's effective speed (override if set, else global).
    TurboSpeed GetEffectiveSpeed(ymir::peripheral::Button button) const;

    /// Check if a button has a per-button speed override.
    bool HasSpeedOverride(ymir::peripheral::Button button) const;

    /// Set the active video standard (for Hz display/conversion). Call when video standard changes.
    void SetVideoStandard(ymir::core::config::sys::VideoStandard standard);

    /// Whether speed configuration mode is active.
    bool IsSpeedModeActive() const { return m_speedMode; }

    /// Set speed configuration mode on or off.
    void SetSpeedMode(bool active);

    /// Reset all turbo state (session end).
    void Reset();

private:
    /// Helper to create default button speeds array (all kNoOverride).
    static constexpr std::array<TurboSpeed, 16> MakeDefaultButtonSpeeds() {
        std::array<TurboSpeed, 16> arr{};
        for (auto &s : arr)
            s = kNoOverride;
        return arr;
    }

    /// Get the effective framesOn for a given bit position.
    uint8_t GetEffectiveFramesOn(int bit) const;

    /// Get the effective pattern length for a given bit position.
    uint8_t GetEffectivePatternLength(int bit) const;

    /// Which buttons have turbo enabled (Button bitmask).
    ymir::peripheral::Button m_turboEnabled = ymir::peripheral::Button::None;

    /// Per-button frame counters indexed by bit position (0–15).
    std::array<uint8_t, 16> m_counters{};

    /// Previous frame's physical button state (for detecting new presses).
    ymir::peripheral::Button m_prevPhysical = ymir::peripheral::Button::Default; // All released

    /// Toggle mode active flag.
    bool m_toggleMode = false;

    /// Current global turbo speed preset.
    TurboSpeed m_globalSpeed = TurboSpeed::Medium;

    /// Per-button speed overrides (kNoOverride = use global).
    std::array<TurboSpeed, 16> m_buttonSpeeds = MakeDefaultButtonSpeeds();

    /// Active video standard for Hz display/conversion.
    ymir::core::config::sys::VideoStandard m_videoStandard = ymir::core::config::sys::VideoStandard::NTSC;

    /// Speed configuration mode active flag.
    bool m_speedMode = false;
};

} // namespace app::input
