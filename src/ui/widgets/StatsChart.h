#ifndef STATSCHART_H
#define STATSCHART_H

#include "Widget.h"

/**
 * Weekly statistics bar chart widget
 *
 * Displays 7 vertical bars representing completed sessions per day:
 * - Monday through Sunday
 * - Auto-scaling to highest value
 * - Value labels on top of bars
 * - Day labels below bars
 *
 * Typical size: 280×120px
 * Bar spacing: 5px
 * Colors: Gradient from red (low) to green (high)
 */
class StatsChart : public Widget {
public:
    StatsChart();

    // Configuration
    void setData(const uint8_t data[7]);  // data[0]=oldest, data[6]=today
    void setMaxValue(uint8_t max);         // Auto-scale if 0
    // 0=Mon..6=Sun. Rotates day labels so the rightmost bar reads as today.
    void setTodayWeekday(uint8_t dow);

    // Widget interface
    void draw(Renderer& renderer) override;

private:
    uint8_t data_[7];
    uint8_t max_value_;
    bool auto_scale_;
    uint8_t today_weekday_ = 6;  // Default: today is Sunday (legacy Mon..Sun layout)

    // Weekday letters indexed 0=Mon..6=Sun
    const char* WEEKDAY_LETTERS[7] = {"M", "T", "W", "T", "F", "S", "S"};
};

#endif // STATSCHART_H
