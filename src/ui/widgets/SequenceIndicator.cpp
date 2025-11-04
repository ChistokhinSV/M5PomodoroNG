#include "SequenceIndicator.h"
#include <M5Unified.h>
#include <math.h>

SequenceIndicator::SequenceIndicator()
    : current_session_(0),
      completed_sessions_(0),
      dots_per_group_(4),
      pulse_phase_(0) {
}

void SequenceIndicator::setSession(uint8_t current, uint8_t completed) {
    if (current_session_ != current || completed_sessions_ != completed) {
        current_session_ = current;
        completed_sessions_ = completed;
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
    const uint8_t MAX_DOTS = 16;
    const uint8_t DOT_SIZE = 6;
    const uint8_t DOT_SPACING = 3;
    const uint8_t GROUP_SPACING = 8;

    uint8_t num_groups = MAX_DOTS / dots_per_group_;
    uint8_t dots_in_group = dots_per_group_;

    // Calculate total width needed
    int16_t total_width = (MAX_DOTS * DOT_SIZE) +
                          ((MAX_DOTS - 1) * DOT_SPACING) +
                          ((num_groups - 1) * GROUP_SPACING);

    // Center the dots horizontally
    int16_t start_x = bounds_.x + (bounds_.w - total_width) / 2;
    int16_t y = bounds_.y + bounds_.h / 2;

    // Draw dots
    int16_t x = start_x;
    for (uint8_t i = 0; i < MAX_DOTS; i++) {
        // Determine dot state
        uint8_t state;
        if (i < completed_sessions_) {
            state = 1;  // Completed
        } else if (i == current_session_) {
            state = 2;  // Current (pulsing)
        } else {
            state = 0;  // Future
        }

        drawDot(renderer, x, y, state);

        // Move to next position
        x += DOT_SIZE + DOT_SPACING;

        // Add group spacing after each group
        if (dots_per_group_ > 1 && (i + 1) % dots_per_group_ == 0 && i < MAX_DOTS - 1) {
            x += GROUP_SPACING;
        }
    }

    clearDirty();
}

void SequenceIndicator::drawDot(Renderer& renderer, int16_t x, int16_t y, uint8_t state) {
    const uint8_t RADIUS = 3;

    switch (state) {
        case 0:  // Future - empty outline
            renderer.drawCircle(x, y, RADIUS, Renderer::Color(0x666666), false);
            break;

        case 1:  // Completed - filled green
            renderer.drawCircle(x, y, RADIUS, Renderer::Color(TFT_GREEN), true);
            break;

        case 2:  // Current - pulsing
            // Draw filled circle
            renderer.drawCircle(x, y, RADIUS, Renderer::Color(TFT_CYAN), true);

            // Draw pulsing ring
            // Calculate pulse radius (3-6px)
            float pulse_t = pulse_phase_ / 360.0f;
            uint8_t pulse_radius = RADIUS + (uint8_t)(pulse_t * 3);

            // Calculate pulse alpha (fade out as radius increases)
            uint8_t alpha = (uint8_t)((1.0f - pulse_t) * 255);

            // Simple ring (just draw outline at pulse radius)
            if (alpha > 50) {  // Only draw if visible enough
                Renderer::Color ring_color = Renderer::Color(TFT_CYAN);
                renderer.drawCircle(x, y, pulse_radius, ring_color, false);
            }
            break;
    }
}
