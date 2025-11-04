#ifndef PROGRESSBAR_H
#define PROGRESSBAR_H

#include "Widget.h"

/**
 * Progress bar widget for timer countdown
 *
 * Features:
 * - Horizontal filled rectangle showing progress (0-100%)
 * - Smooth animation between progress values
 * - Optional percentage text overlay
 * - Configurable color
 * - Gradient fill (darker at edges)
 *
 * Typical size: 280×20px
 * Animation: Linear ease over 500ms
 */
class ProgressBar : public Widget {
public:
    ProgressBar();

    // Configuration
    void setProgress(uint8_t percent);  // 0-100
    void setColor(Renderer::Color color);
    void setShowPercentage(bool show);

    // Widget interface
    void draw(Renderer& renderer) override;
    void update(uint32_t deltaMs) override;

private:
    uint8_t current_progress_;
    uint8_t target_progress_;
    Renderer::Color color_;
    bool show_percentage_;

    // Animation
    uint32_t anim_elapsed_ms_;
    static constexpr uint32_t ANIM_DURATION_MS = 300;
};

#endif // PROGRESSBAR_H
