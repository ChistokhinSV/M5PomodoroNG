#ifndef SEQUENCEINDICATOR_H
#define SEQUENCEINDICATOR_H

#include "Widget.h"

/**
 * Pomodoro sequence indicator widget
 *
 * Displays session progress using dots:
 * - Classic mode: 4 groups of 4 dots (●●●● ●●●● ●●●● ●●●●)
 * - Study mode: 16 individual dots
 *
 * Dot states:
 * - Current session: Pulsing ring animation
 * - Completed: Filled (green)
 * - Future: Empty outline (gray)
 *
 * Typical size: 200×20px
 */
class SequenceIndicator : public Widget {
public:
    SequenceIndicator();

    // Configuration
    void setSession(uint8_t current, uint8_t completed);  // 0-15
    void setDotsPerGroup(uint8_t count);  // Classic=4, Study=1

    // Widget interface
    void draw(Renderer& renderer) override;
    void update(uint32_t deltaMs) override;

private:
    uint8_t current_session_;
    uint8_t completed_sessions_;
    uint8_t dots_per_group_;

    // Animation for current dot
    uint16_t pulse_phase_;  // 0-360 degrees
    static constexpr uint16_t PULSE_SPEED = 180;  // degrees per second

    void drawDot(Renderer& renderer, int16_t x, int16_t y, uint8_t state);
    // Dot states: 0=future, 1=completed, 2=current
};

#endif // SEQUENCEINDICATOR_H
