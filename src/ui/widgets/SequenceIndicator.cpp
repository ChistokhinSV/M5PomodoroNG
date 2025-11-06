#include "SequenceIndicator.h"
#include <M5Unified.h>
#include <math.h>

SequenceIndicator::SequenceIndicator()
    : current_session_(0),
      completed_sessions_(0),
      dots_per_group_(4),
      total_sessions_(8),  // Default to classic mode (4 work sessions)
      in_break_(false),
      pulse_phase_(0) {
}

void SequenceIndicator::setSession(uint8_t current, uint8_t completed, bool in_break) {
    if (current_session_ != current || completed_sessions_ != completed || in_break_ != in_break) {
        current_session_ = current;
        completed_sessions_ = completed;
        in_break_ = in_break;
        markDirty();
    }
}

void SequenceIndicator::setDotsPerGroup(uint8_t count) {
    if (count == 0) count = 1;
    if (dots_per_group_ != count) {
        dots_per_group_ = count;
        markDirty();
    }
}

void SequenceIndicator::setTotalSessions(uint8_t total) {
    if (total == 0) total = 1;
    if (total > 16) total = 16;  // Max 16 dots supported
    if (total_sessions_ != total) {
        total_sessions_ = total;
        markDirty();
    }
}

void SequenceIndicator::update(uint32_t deltaMs) {
    // Update pulse animation
    pulse_phase_ += (PULSE_SPEED * deltaMs) / 1000;
    if (pulse_phase_ >= 360) {
        pulse_phase_ -= 360;
    }
    markDirty();
}

void SequenceIndicator::draw(Renderer& renderer) {
    if (!visible_) return;

    // Calculate layout
    const uint8_t DOT_SIZE = 6;
    const uint8_t DOT_SPACING = 3;
    const uint8_t GROUP_SPACING = 8;

    uint8_t num_groups = total_sessions_ / dots_per_group_;
    if (total_sessions_ % dots_per_group_ != 0) num_groups++;  // Round up

    // Calculate total width needed
    int16_t total_width = (total_sessions_ * DOT_SIZE) +
                          ((total_sessions_ - 1) * DOT_SPACING) +
                          ((num_groups - 1) * GROUP_SPACING);

    // Center the dots horizontally
    int16_t start_x = bounds_.x + (bounds_.w - total_width) / 2;
    int16_t y = bounds_.y + bounds_.h / 2;

    // Draw dots
    int16_t x = start_x;
    for (uint8_t i = 0; i < total_sessions_; i++) {
        // Determine dot state
        uint8_t state;
        if (in_break_) {
            // During break: pulse the last completed work session
            if (i < completed_sessions_ - 1) {
                state = 1;  // Completed (green)
            } else if (i == completed_sessions_ - 1) {
                state = 3;  // Resting after this session (green with pulse)
            } else {
                state = 0;  // Future (gray outline)
            }
        } else {
            // During work: pulse the current work session
            if (i < completed_sessions_) {
                state = 1;  // Completed (green)
            } else if (i == current_session_) {
                state = 2;  // Current work (white with pulse)
            } else {
                state = 0;  // Future (gray outline)
            }
        }

        drawDot(renderer, x, y, state);

        // Move to next position
        x += DOT_SIZE + DOT_SPACING;

        // Add group spacing after each group
        if (dots_per_group_ > 1 && (i + 1) % dots_per_group_ == 0 && i < total_sessions_ - 1) {
            x += GROUP_SPACING;
        }
    }

    clearDirty();
}

void SequenceIndicator::drawDot(Renderer& renderer, int16_t x, int16_t y, uint8_t state) {
    const uint8_t RADIUS = 3;

    switch (state) {
        case 0:  // Future - empty outline (gray)
            renderer.drawCircle(x, y, RADIUS, Renderer::Color(0x632C), false);  // Gray in RGB565
            break;

        case 1:  // Completed - filled green
            renderer.drawCircle(x, y, RADIUS, Renderer::Color(TFT_GREEN), true);
            break;

        case 2: {  // Current work session - white with pulsing ring
            // Draw filled white circle
            renderer.drawCircle(x, y, RADIUS, Renderer::Color(TFT_WHITE), true);

            // Draw pulsing ring
            // Calculate pulse radius (3-6px)
            float pulse_t = pulse_phase_ / 360.0f;
            uint8_t pulse_radius = RADIUS + (uint8_t)(pulse_t * 3);

            // Calculate pulse alpha (fade out as radius increases)
            uint8_t alpha = (uint8_t)((1.0f - pulse_t) * 255);

            // Simple ring (just draw outline at pulse radius)
            if (alpha > 50) {  // Only draw if visible enough
                Renderer::Color ring_color = Renderer::Color(TFT_WHITE);
                renderer.drawCircle(x, y, pulse_radius, ring_color, false);
            }
            break;
        }

        case 3: {  // Resting after work session - green with pulsing ring
            // Draw filled green circle
            renderer.drawCircle(x, y, RADIUS, Renderer::Color(TFT_GREEN), true);

            // Draw pulsing ring
            float pulse_t3 = pulse_phase_ / 360.0f;
            uint8_t pulse_radius3 = RADIUS + (uint8_t)(pulse_t3 * 3);
            uint8_t alpha3 = (uint8_t)((1.0f - pulse_t3) * 255);

            if (alpha3 > 50) {
                Renderer::Color ring_color = Renderer::Color(TFT_GREEN);
                renderer.drawCircle(x, y, pulse_radius3, ring_color, false);
            }
            break;
        }
    }
}
