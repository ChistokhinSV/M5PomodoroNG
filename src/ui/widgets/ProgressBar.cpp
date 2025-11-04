#include "ProgressBar.h"
#include <M5Unified.h>
#include <stdio.h>

ProgressBar::ProgressBar()
    : current_progress_(0),
      target_progress_(0),
      color_(Renderer::Color(TFT_RED)),
      show_percentage_(true),
      anim_elapsed_ms_(0) {
}

void ProgressBar::setProgress(uint8_t percent) {
    if (percent > 100) percent = 100;

    if (target_progress_ != percent) {
        target_progress_ = percent;
        anim_elapsed_ms_ = 0;  // Restart animation
        markDirty();
    }
}

void ProgressBar::setColor(Renderer::Color color) {
    color_ = color;
    markDirty();
}

void ProgressBar::setShowPercentage(bool show) {
    if (show_percentage_ != show) {
        show_percentage_ = show;
        markDirty();
    }
}

void ProgressBar::update(uint32_t deltaMs) {
    if (current_progress_ != target_progress_) {
        anim_elapsed_ms_ += deltaMs;

        if (anim_elapsed_ms_ >= ANIM_DURATION_MS) {
            // Animation complete
            current_progress_ = target_progress_;
            anim_elapsed_ms_ = ANIM_DURATION_MS;
        } else {
            // Linear interpolation
            int16_t progress_delta = target_progress_ - current_progress_;
            current_progress_ = current_progress_ +
                (progress_delta * anim_elapsed_ms_) / ANIM_DURATION_MS;
        }

        markDirty();
    }
}

void ProgressBar::draw(Renderer& renderer) {
    if (!visible_) return;

    // Draw background (empty bar)
    renderer.drawRect(bounds_.x, bounds_.y, bounds_.w, bounds_.h,
                     Renderer::Color(0x333333), true);

    // Draw border
    renderer.drawRect(bounds_.x, bounds_.y, bounds_.w, bounds_.h,
                     Renderer::Color(0x666666), false);

    // Draw filled portion
    if (current_progress_ > 0) {
        int16_t fill_width = (bounds_.w * current_progress_) / 100;
        if (fill_width > 0) {
            renderer.drawRect(bounds_.x + 1, bounds_.y + 1,
                             fill_width - 2, bounds_.h - 2,
                             color_, true);
        }
    }

    // Draw percentage text overlay
    if (show_percentage_) {
        char percent_str[5];
        snprintf(percent_str, sizeof(percent_str), "%d%%", current_progress_);

        renderer.setTextDatum(MC_DATUM);  // Middle-center
        int16_t center_x = bounds_.x + bounds_.w / 2;
        int16_t center_y = bounds_.y + bounds_.h / 2;

        // Draw text with shadow for visibility
        renderer.drawString(center_x + 1, center_y + 1, percent_str,
                           &fonts::Font2, Renderer::Color(TFT_BLACK));
        renderer.drawString(center_x, center_y, percent_str,
                           &fonts::Font2, Renderer::Color(TFT_WHITE));
    }

    clearDirty();
}
