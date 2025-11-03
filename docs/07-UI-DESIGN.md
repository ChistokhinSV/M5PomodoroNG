# UI Design Specification

## 1. Overview

This document specifies the user interface design for the M5 Pomodoro Timer v2, including screen layouts, visual design system, widget specifications, and navigation flows for the M5Stack Core2's 320×240 pixel IPS display.

### Design Goals

1. **High Visibility**: Clear readability in various lighting conditions
2. **Minimal Cognitive Load**: Essential information at a glance
3. **Touch-Optimized**: Large touch targets (min 44×44px) for reliable interaction
4. **Performance**: Maintain 10+ FPS with dirty rectangle rendering
5. **Battery Efficiency**: Minimize full-screen redraws, use dark backgrounds

### Display Specifications

```
Resolution:    320×240 pixels (QVGA)
Aspect Ratio:  4:3
Color Depth:   16-bit RGB565 (65,536 colors)
Orientation:   Landscape
Touch Type:    FT6336U capacitive (max 2 simultaneous touches)
```

## 2. Design System

### Color Palette

Primary colors optimized for readability and battery efficiency:

```cpp
// Primary Colors (RGB565)
#define COLOR_BG_DARK       0x0000  // Black (battery efficient)
#define COLOR_BG_CARD       0x2124  // Dark gray (#202020)
#define COLOR_PRIMARY       0xFBE0  // Pomodoro red (#FF5555)
#define COLOR_SUCCESS       0x07E0  // Green (#00FF00)
#define COLOR_WARNING       0xFE60  // Orange (#FFC800)
#define COLOR_INFO          0x05FF  // Cyan (#00BFFF)

// Text Colors
#define COLOR_TEXT_PRIMARY  0xFFFF  // White
#define COLOR_TEXT_SECONDARY 0xAD55 // Light gray (#AAAAAA)
#define COLOR_TEXT_DISABLED 0x5ACB  // Medium gray (#555555)

// State-Specific Colors
#define COLOR_POMODORO      0xFBE0  // Red
#define COLOR_SHORT_REST    0x07E0  // Green
#define COLOR_LONG_REST     0x05FF  // Cyan
#define COLOR_PAUSED        0xFE60  // Orange
#define COLOR_STOPPED       0xAD55  // Gray
```

### Typography

```cpp
// Font Configuration (M5Unified built-in fonts)
#define FONT_LARGE    &fonts::Orbitron_Light_32  // Timer display
#define FONT_MEDIUM   &fonts::Font4              // Session info
#define FONT_SMALL    &fonts::Font2              // Labels/stats
#define FONT_TINY     &fonts::Font0              // Timestamps

// Text Sizes
Large:   32pt (timer countdown)
Medium:  24pt (session labels, stats)
Small:   16pt (buttons, labels)
Tiny:    8pt  (status bar, timestamps)
```

### Layout Grid

```
Screen: 320×240px
Margins: 8px (left/right/top/bottom)
Column gutter: 8px
Component spacing: 12px vertical

Safe area: 304×224px (accounting for margins)
Touch target minimum: 44×44px
```

## 3. Screen Layouts

### 3.1 Main Screen (Default)

**Purpose**: Display timer countdown and current session state

```
┌────────────────────────────────────────────────────────┐
│ [StatusBar: 320×20px]                                  │
│ Battery:85% │ WiFi:● │ Mode:BAL │ Time:14:23          │
├────────────────────────────────────────────────────────┤
│                                                        │
│              POMODORO SESSION                          │
│                  [Session 1/4]                         │
│                                                        │
│                    24:35                               │
│              [Large Timer Text]                        │
│                                                        │
│   ┌──────────────────────────────────────────┐        │
│   │ Progress: ████████████░░░░░░░░░░░░ 48%   │        │
│   └──────────────────────────────────────────┘        │
│                                                        │
│   [●●●●] [○○○○] [○○○○] [○○○○]                        │
│   [Sequence Indicator - 4 groups of 4 dots]           │
│                                                        │
│  ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐     │
│  │ STATS  │  │ PAUSE  │  │ STOP   │  │  SET   │     │
│  └────────┘  └────────┘  └────────┘  └────────┘     │
│  [Touch buttons: 70×40px each]                        │
└────────────────────────────────────────────────────────┘

Dimensions:
- StatusBar: 320×20px (top)
- Session label: centered, y=30
- Timer text: centered, y=80, FONT_LARGE
- Progress bar: 280×20px, centered, y=130
- Sequence indicator: centered, y=160
- Button row: 4 buttons, 70×40px, y=190, spacing=8px
```

**State-Specific Variations**:

1. **STOPPED State**:
   - Timer shows: "25:00" (default Pomodoro duration)
   - Session label: "Ready to Start"
   - Progress bar: 0%
   - Sequence indicator: All dots empty
   - Buttons: [STATS] [START] [SET]

2. **POMODORO State** (running):
   - Timer counts down: "24:35", "24:34"...
   - Session label: "POMODORO SESSION" (red text)
   - Progress bar updates every second
   - Sequence indicator: Current dot filled
   - Buttons: [STATS] [PAUSE] [STOP] [SET]

3. **SHORT_REST State**:
   - Timer counts down: "04:58"...
   - Session label: "SHORT REST" (green text)
   - Progress bar updates
   - Sequence indicator: Completed Pomodoro filled
   - Buttons: [STATS] [PAUSE] [STOP] [SET]

4. **LONG_REST State**:
   - Timer counts down: "14:42"...
   - Session label: "LONG REST" (cyan text)
   - Sequence indicator: All 4 Pomodoros filled
   - Buttons: [STATS] [PAUSE] [STOP] [SET]

5. **PAUSED State**:
   - Timer frozen: "12:34"
   - Session label: "PAUSED" (orange text)
   - Buttons: [STATS] [RESUME] [STOP] [SET]

### 3.2 Statistics Screen

**Purpose**: Display daily and weekly statistics with charts

```
┌────────────────────────────────────────────────────────┐
│ [StatusBar: 320×20px]                                  │
├────────────────────────────────────────────────────────┤
│                  TODAY'S STATS                         │
│                  Dec 15, 2025                          │
│                                                        │
│   Completed: 6 🍅    Started: 7                        │
│   Work Time: 2h 30m  Interruptions: 1                  │
│   Completion: 85.7%  Streak: 3 days                    │
│                                                        │
│  ┌────────────────────────────────────────────┐       │
│  │  7-DAY COMPLETION RATE                     │       │
│  │  ┌─┬─┬─┬─┬─┬─┬─┐                           │       │
│  │  │█│█│░│█│█│█│█│  85% average              │       │
│  │  └─┴─┴─┴─┴─┴─┴─┘                           │       │
│  │  M T W T F S S                             │       │
│  └────────────────────────────────────────────┘       │
│                                                        │
│  ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐     │
│  │ PREV   │  │ NEXT   │  │ WEEK   │  │  BACK  │     │
│  └────────┘  └────────┘  └────────┘  └────────┘     │
└────────────────────────────────────────────────────────┘

Dimensions:
- Title: centered, y=30
- Date: centered, y=50, FONT_SMALL
- Stats grid: 2×3, starts y=70
- Chart area: 280×60px, y=130
- Button row: y=200
```

**Weekly View** (toggled with WEEK button):

```
│                  WEEKLY STATS                          │
│              Dec 9 - Dec 15, 2025                      │
│                                                        │
│   Completed: 38 🍅   Started: 42                       │
│   Work Time: 15h 50m Interruptions: 4                  │
│   Completion: 90.5%  Best Day: Mon (8🍅)               │
│                                                        │
│  ┌────────────────────────────────────────────┐       │
│  │  DAILY BREAKDOWN                           │       │
│  │  M: ████████ 8                             │       │
│  │  T: ██████ 6                               │       │
│  │  W: ███ 3                                  │       │
│  │  T: ██████ 6                               │       │
│  │  F: ███████ 7                              │       │
│  │  S: ████ 4                                 │       │
│  │  S: ████ 4                                 │       │
│  └────────────────────────────────────────────┘       │
```

### 3.3 Settings Screen

**Purpose**: Configure timer durations, modes, sensitivity, and integrations

```
┌────────────────────────────────────────────────────────┐
│ [StatusBar]                                            │
├────────────────────────────────────────────────────────┤
│                    SETTINGS                            │
│                                                        │
│  ┌────────────────────────────────────────────┐       │
│  │ TIMER SETTINGS                             │       │
│  │  Pomodoro Duration:      [25] min          │       │
│  │  Short Rest Duration:    [ 5] min          │       │
│  │  Long Rest Duration:     [15] min          │       │
│  │  Study Mode (45-15):     [OFF]             │       │
│  └────────────────────────────────────────────┘       │
│                                                        │
│  ┌────────────────────────────────────────────┐       │
│  │ POWER & SENSITIVITY                        │       │
│  │  Power Mode: [BALANCED] ▼                  │       │
│  │  Gyro Sensitivity: [60°] ▼                 │       │
│  │  Gyro Wake: [ON]                           │       │
│  └────────────────────────────────────────────┘       │
│                                                        │
│  ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐     │
│  │  MORE  │  │ CLOUD  │  │ RESET  │  │  BACK  │     │
│  └────────┘  └────────┘  └────────┘  └────────┘     │
└────────────────────────────────────────────────────────┘

Dimensions:
- Title: centered, y=25
- Card 1 (Timer): 304×70px, y=45
- Card 2 (Power): 304×50px, y=125
- Button row: y=200
```

**MORE Settings** (second page):

```
│                    SETTINGS (2/2)                      │
│                                                        │
│  ┌────────────────────────────────────────────┐       │
│  │ DISPLAY & FEEDBACK                         │       │
│  │  Brightness:        [●●●●○] 80%            │       │
│  │  LED Feedback:      [ON]                   │       │
│  │  Sound Alerts:      [OFF]                  │       │
│  └────────────────────────────────────────────┘       │
│                                                        │
│  ┌────────────────────────────────────────────┐       │
│  │ SYSTEM                                     │       │
│  │  Firmware: v2.0.0                          │       │
│  │  Free Memory: 485KB / 520KB                │       │
│  │  Uptime: 2h 14m                            │       │
│  └────────────────────────────────────────────┘       │
```

**CLOUD Settings** (popup/overlay):

```
│  ┌────────────────────────────────────────────┐       │
│  │ CLOUD SYNC                                 │       │
│  │                                            │       │
│  │  AWS IoT: [CONNECTED] ●                    │       │
│  │  Last Sync: 2 min ago                      │       │
│  │                                            │       │
│  │  Toggl Integration:  [ENABLED]             │       │
│  │  Calendar Sync:      [ENABLED]             │       │
│  │                                            │       │
│  │  ┌──────────┐  ┌──────────┐               │       │
│  │  │   SYNC   │  │  CLOSE   │               │       │
│  │  └──────────┘  └──────────┘               │       │
│  └────────────────────────────────────────────┘       │
```

### 3.4 Pause Screen (Overlay)

**Purpose**: Display pause state with resume options

```
┌────────────────────────────────────────────────────────┐
│ [Main Screen dimmed 50% in background]                 │
│                                                        │
│          ┌──────────────────────────┐                 │
│          │                          │                 │
│          │        PAUSED            │                 │
│          │                          │                 │
│          │        12:34             │                 │
│          │    [Timer frozen]        │                 │
│          │                          │                 │
│          │  Lay flat to resume      │                 │
│          │  or tap RESUME           │                 │
│          │                          │                 │
│          │  ┌────────┐  ┌────────┐ │                 │
│          │  │ RESUME │  │  STOP  │ │                 │
│          │  └────────┘  └────────┘ │                 │
│          │                          │                 │
│          └──────────────────────────┘                 │
│                                                        │
└────────────────────────────────────────────────────────┘

Dimensions:
- Overlay card: 240×180px, centered
- Background: 50% opacity black
- Timer: FONT_LARGE, centered
- Instruction text: FONT_SMALL, centered
- Buttons: 100×44px, spacing=12px
```

### 3.5 Toast Notifications

**Purpose**: Brief feedback messages (auto-dismiss after 3 seconds)

```
┌────────────────────────────────────────────────────────┐
│ [Main Screen]                                          │
│                                                        │
│          ┌──────────────────────────┐                 │
│          │ ✓ Session completed!     │                 │
│          └──────────────────────────┘                 │
│                      [y=200]                           │
│                                                        │
└────────────────────────────────────────────────────────┘

Types:
- Success: Green background, checkmark icon
- Warning: Orange background, exclamation icon
- Error: Red background, X icon
- Info: Cyan background, i icon

Dimensions: 240×40px, centered horizontally, y=200
Auto-dismiss: 3 seconds
```

## 4. Widget Specifications

### 4.1 StatusBar Widget

**Purpose**: Always-visible status indicators

```cpp
class StatusBar {
    void render(int16_t x, int16_t y);
    void updateBattery(uint8_t percent);
    void updateWiFi(bool connected);
    void updateMode(PowerMode mode);
    void updateTime(uint8_t hour, uint8_t minute);
};

// Render example
void StatusBar::render(int16_t x, int16_t y) {
    // Background
    M5.Display.fillRect(x, y, 320, 20, COLOR_BG_CARD);

    // Battery: left side
    uint8_t battPct = getBatteryPercent();
    M5.Display.setFont(FONT_TINY);
    M5.Display.setTextColor(battPct > 20 ? COLOR_SUCCESS : COLOR_WARNING);
    M5.Display.drawString(String("Bat:") + battPct + "%", x + 4, y + 6);

    // WiFi: center-left
    M5.Display.setTextColor(wifiConnected ? COLOR_SUCCESS : COLOR_TEXT_DISABLED);
    M5.Display.drawString("WiFi:●", x + 80, y + 6);

    // Power mode: center-right
    M5.Display.setTextColor(COLOR_TEXT_SECONDARY);
    M5.Display.drawString("Mode:" + getPowerModeStr(), x + 150, y + 6);

    // Time: right side
    M5.Display.setTextColor(COLOR_TEXT_PRIMARY);
    M5.Display.drawString(getTimeStr(), x + 250, y + 6);
}
```

**Update Strategy**: Only redraw changed elements (battery every 60s, WiFi on status change, time every minute)

### 4.2 TimerDisplay Widget

**Purpose**: Large countdown timer

```cpp
class TimerDisplay {
    void render(int16_t x, int16_t y, uint16_t remainingSeconds);
    void setColor(uint16_t color);
    void setFlashing(bool enabled);  // For last 10 seconds
};

void TimerDisplay::render(int16_t x, int16_t y, uint16_t remainingSeconds) {
    uint8_t minutes = remainingSeconds / 60;
    uint8_t seconds = remainingSeconds % 60;

    // Format: "MM:SS"
    char timeStr[6];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", minutes, seconds);

    // Clear previous (dirty rectangle)
    M5.Display.fillRect(x - 60, y - 20, 120, 40, COLOR_BG_DARK);

    // Draw time
    M5.Display.setFont(FONT_LARGE);
    M5.Display.setTextColor(currentColor);
    M5.Display.setTextDatum(MC_DATUM);  // Middle-center
    M5.Display.drawString(timeStr, x, y);

    // Flash effect for last 10 seconds
    if (flashing && (remainingSeconds % 2 == 0)) {
        M5.Display.setTextColor(COLOR_WARNING);
        M5.Display.drawString(timeStr, x, y);
    }
}
```

**Optimization**: Use sprite for timer to avoid full-screen redraw (80×40px sprite)

### 4.3 ProgressBar Widget

**Purpose**: Visual session progress indicator

```cpp
class ProgressBar {
    void render(int16_t x, int16_t y, uint8_t percent);
    void setColor(uint16_t color);
};

void ProgressBar::render(int16_t x, int16_t y, uint8_t percent) {
    const int16_t width = 280;
    const int16_t height = 20;
    const int16_t radius = 4;  // Rounded corners

    // Clear previous
    M5.Display.fillRect(x, y, width, height, COLOR_BG_DARK);

    // Background (empty part)
    M5.Display.fillRoundRect(x, y, width, height, radius, COLOR_BG_CARD);

    // Filled part
    int16_t fillWidth = (width - 4) * percent / 100;
    if (fillWidth > 0) {
        M5.Display.fillRoundRect(x + 2, y + 2, fillWidth, height - 4, radius, barColor);
    }

    // Border
    M5.Display.drawRoundRect(x, y, width, height, radius, COLOR_TEXT_SECONDARY);

    // Percentage text
    M5.Display.setFont(FONT_TINY);
    M5.Display.setTextColor(COLOR_TEXT_PRIMARY);
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.drawString(String(percent) + "%", x + width / 2, y + height / 2);
}
```

**Update Strategy**: Only update when percentage changes (every second during timer)

### 4.4 SequenceIndicator Widget

**Purpose**: Show Pomodoro sequence progress (4 sessions)

```cpp
class SequenceIndicator {
    void render(int16_t x, int16_t y, uint8_t currentSession, uint8_t completedSessions);
};

void SequenceIndicator::render(int16_t x, int16_t y, uint8_t currentSession, uint8_t completedSessions) {
    const int16_t dotSize = 12;
    const int16_t groupSpacing = 20;
    const int16_t dotSpacing = 4;

    int16_t currentX = x;

    for (uint8_t group = 0; group < 4; group++) {
        for (uint8_t dot = 0; dot < 4; dot++) {
            uint8_t sessionNum = group * 4 + dot;

            // Determine fill state
            bool isCurrent = (sessionNum == currentSession);
            bool isCompleted = (sessionNum < completedSessions);

            if (isCompleted) {
                M5.Display.fillCircle(currentX, y, dotSize / 2, COLOR_SUCCESS);
            } else if (isCurrent) {
                M5.Display.fillCircle(currentX, y, dotSize / 2, COLOR_PRIMARY);
                M5.Display.drawCircle(currentX, y, dotSize / 2 + 2, COLOR_PRIMARY);  // Pulsing ring
            } else {
                M5.Display.drawCircle(currentX, y, dotSize / 2, COLOR_TEXT_DISABLED);
            }

            currentX += dotSize + dotSpacing;
        }
        currentX += groupSpacing;
    }
}
```

**Animation**: Current session dot pulses (ring expands/contracts every 500ms)

### 4.5 Button Widget

**Purpose**: Touch-responsive buttons

```cpp
class Button {
    Button(int16_t x, int16_t y, int16_t w, int16_t h, const char* label);
    void render(bool pressed = false);
    bool contains(int16_t touchX, int16_t touchY);
};

void Button::render(bool pressed) {
    uint16_t bgColor = pressed ? COLOR_PRIMARY : COLOR_BG_CARD;
    uint16_t textColor = pressed ? COLOR_BG_DARK : COLOR_TEXT_PRIMARY;

    // Background
    M5.Display.fillRoundRect(x, y, width, height, 6, bgColor);

    // Border
    M5.Display.drawRoundRect(x, y, width, height, 6, COLOR_TEXT_SECONDARY);

    // Label
    M5.Display.setFont(FONT_SMALL);
    M5.Display.setTextColor(textColor);
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.drawString(label, x + width / 2, y + height / 2);
}

bool Button::contains(int16_t touchX, int16_t touchY) {
    return (touchX >= x && touchX <= x + width &&
            touchY >= y && touchY <= y + height);
}
```

**Touch Feedback**:
- Visual: Button background changes to primary color on press
- Haptic: Optional vibration motor pulse (50ms)
- Audio: Optional beep (if enabled in settings)

### 4.6 StatsChart Widget

**Purpose**: Bar chart for 7-day statistics

```cpp
class StatsChart {
    void render(int16_t x, int16_t y, uint8_t data[7], uint8_t maxValue);
};

void StatsChart::render(int16_t x, int16_t y, uint8_t data[7], uint8_t maxValue) {
    const int16_t chartWidth = 280;
    const int16_t chartHeight = 60;
    const int16_t barWidth = 30;
    const int16_t barSpacing = 10;

    // Clear area
    M5.Display.fillRect(x, y, chartWidth, chartHeight + 20, COLOR_BG_DARK);

    // Draw bars
    for (uint8_t i = 0; i < 7; i++) {
        int16_t barX = x + i * (barWidth + barSpacing);
        int16_t barHeight = (chartHeight * data[i]) / maxValue;
        int16_t barY = y + chartHeight - barHeight;

        // Bar
        M5.Display.fillRect(barX, barY, barWidth, barHeight, COLOR_PRIMARY);

        // Value label
        M5.Display.setFont(FONT_TINY);
        M5.Display.setTextColor(COLOR_TEXT_SECONDARY);
        M5.Display.setTextDatum(TC_DATUM);
        M5.Display.drawString(String(data[i]), barX + barWidth / 2, barY - 10);

        // Day label
        const char* days[] = {"M", "T", "W", "T", "F", "S", "S"};
        M5.Display.drawString(days[i], barX + barWidth / 2, y + chartHeight + 5);
    }
}
```

## 5. Navigation Flow

### Touch Navigation

```
Main Screen
├─ [STATS] → Statistics Screen
│            ├─ [PREV/NEXT] → Navigate days
│            ├─ [WEEK] → Toggle weekly view
│            └─ [BACK] → Main Screen
│
├─ [START/PAUSE/RESUME] → Toggle timer state
│                         → Shows toast notification
│
├─ [STOP] → Confirmation dialog
│           ├─ [YES] → Stop timer, return to STOPPED
│           └─ [NO] → Return to current state
│
└─ [SET] → Settings Screen
            ├─ [MORE] → Settings page 2
            ├─ [CLOUD] → Cloud sync popup
            ├─ [RESET] → Factory reset confirmation
            └─ [BACK] → Main Screen
```

### Gyro Navigation

```
Main Screen (STOPPED)
└─ Rotate ±60° → Auto-start timer (gyro wake enabled)

Main Screen (POMODORO/REST)
└─ Rotate ±60° → Pause (show Pause Screen overlay)

Pause Screen
├─ Lay flat (1s hold) → Resume timer
└─ Double-shake → Emergency stop → STOPPED
```

### Encoder Navigation (Optional)

M5Stack Core2 doesn't have physical encoder, but if using HMI module:

```
Any Screen
├─ Rotate encoder → Navigate between buttons (highlight)
└─ Press encoder → Activate highlighted button
```

## 6. Rendering Pipeline

### Frame Update Strategy

```cpp
void UIManager::update() {
    // Step 1: Check for state changes
    if (timerStateChanged) {
        fullRedraw();  // State transition requires full redraw
        timerStateChanged = false;
        return;
    }

    // Step 2: Update only changed elements (dirty rectangles)
    if (timerRunning && (millis() - lastTimerUpdate > 1000)) {
        updateTimerDisplay();    // Sprite update (~5ms)
        updateProgressBar();     // Dirty rectangle (~3ms)
        lastTimerUpdate = millis();
    }

    if (millis() - lastStatusUpdate > 60000) {
        updateStatusBar();       // Battery update (~2ms)
        lastStatusUpdate = millis();
    }

    // Step 3: Handle touch input
    processTouchInput();

    // Step 4: Render animations (if any)
    if (sequenceIndicatorPulsing) {
        updateSequenceIndicatorAnimation();  // (~3ms)
    }
}
```

### Performance Budget

```
Target frame time: 100ms (10 FPS minimum)

Dirty rectangle updates:
- Timer display:        5ms
- Progress bar:         3ms
- Status bar:           2ms
- Sequence indicator:   3ms
- Button highlight:     2ms
Total:                 15ms (85ms headroom)

Full screen redraw:    45ms (acceptable for state transitions)
```

### Sprite Optimization

```cpp
// Create sprites for frequently updated elements
LGFX_Sprite timerSprite(&M5.Display);
LGFX_Sprite progressSprite(&M5.Display);

void setup() {
    // Timer sprite: 120×40px @ 16-bit
    timerSprite.createSprite(120, 40);
    timerSprite.setColorDepth(16);

    // Progress sprite: 280×20px @ 16-bit
    progressSprite.createSprite(280, 20);
    progressSprite.setColorDepth(16);
}

void updateTimerDisplay() {
    // Render to sprite
    timerSprite.fillSprite(COLOR_BG_DARK);
    timerSprite.setFont(FONT_LARGE);
    timerSprite.setTextColor(COLOR_PRIMARY);
    timerSprite.drawString("24:35", 60, 20);

    // Push sprite to screen (hardware-accelerated)
    timerSprite.pushSprite(100, 80);  // Centered position
}
```

**Memory Cost**:
- Timer sprite: 120 × 40 × 2 bytes = 9,600 bytes
- Progress sprite: 280 × 20 × 2 bytes = 11,200 bytes
- Total: ~21KB (acceptable)

## 7. Accessibility Considerations

### High Contrast Mode

```cpp
// Optional high-contrast palette (configurable in settings)
#define COLOR_BG_DARK_HC       0x0000  // Pure black
#define COLOR_TEXT_PRIMARY_HC  0xFFFF  // Pure white
#define COLOR_PRIMARY_HC       0xF800  // Pure red
#define COLOR_SUCCESS_HC       0x07E0  // Pure green
#define COLOR_WARNING_HC       0xFFE0  // Yellow
```

### Large Touch Targets

All interactive elements meet 44×44px minimum size:
- Buttons: 70×40px (exceeds minimum)
- Settings toggles: 60×44px
- Chart bars: 30×variable height (acceptable for non-critical)

### Visual Feedback

Every interaction provides feedback:
- Touch: Button color change + optional haptic
- Gyro: Toast notification ("Timer paused by rotation")
- Timer events: Color changes + LED feedback

### Low Battery Warning

```cpp
void checkBatteryWarning() {
    uint8_t battPct = getBatteryPercent();

    if (battPct <= 10 && !lowBatteryWarningShown) {
        showToast("Low battery: 10%", ToastType::WARNING);
        lowBatteryWarningShown = true;

        // Flash LED red
        setLEDColor(COLOR_WARNING, 3);  // 3 flashes
    }

    if (battPct <= 5) {
        // Force BATTERY_SAVER mode
        setPowerMode(PowerMode::BATTERY_SAVER);
        showToast("Battery critical: Power saver enabled", ToastType::ERROR);
    }
}
```

## 8. Screen Transition Animations

### Fade Transition (Default)

```cpp
void UIManager::transitionToScreen(Screen* newScreen) {
    const uint8_t fadeSteps = 10;
    const uint16_t fadeDelay = 20;  // ms

    // Fade out current screen
    for (uint8_t alpha = 255; alpha > 0; alpha -= 25) {
        M5.Display.setBrightness(alpha);
        delay(fadeDelay);
    }

    // Switch screen
    currentScreen = newScreen;
    currentScreen->render();

    // Fade in new screen
    for (uint8_t alpha = 0; alpha < 255; alpha += 25) {
        M5.Display.setBrightness(alpha);
        delay(fadeDelay);
    }
}
```

**Duration**: 400ms total (acceptable for UX)

### Slide Transition (Optional)

```cpp
void UIManager::slideToScreen(Screen* newScreen, SlideDirection direction) {
    // Capture current screen to sprite
    LGFX_Sprite currentSprite(&M5.Display);
    currentSprite.createSprite(320, 240);
    currentSprite.readRect(0, 0, 320, 240, (uint16_t*)currentSprite.getBuffer());

    // Render new screen to sprite
    LGFX_Sprite newSprite(&M5.Display);
    newSprite.createSprite(320, 240);
    newScreen->renderToSprite(&newSprite);

    // Slide animation
    for (int16_t offset = 0; offset < 320; offset += 32) {
        if (direction == SlideDirection::LEFT) {
            currentSprite.pushSprite(-offset, 0);
            newSprite.pushSprite(320 - offset, 0);
        }
        delay(16);  // ~60 FPS
    }

    currentScreen = newScreen;
}
```

**Note**: Slide transitions consume ~150KB RAM (2× full-screen sprites), use sparingly

## 9. Mockups

### Main Screen States (Detailed)

**STOPPED State:**
```
┌────────────────────────────────────────────────┐
│ Bat:85% │ WiFi:● │ Mode:BAL │ Time:14:23     │ ← StatusBar (gray bg)
├────────────────────────────────────────────────┤
│                                                │
│              Ready to Start                    │ ← Session label (gray)
│                                                │
│                   25:00                        │ ← Timer (white, large)
│                                                │
│  ┌────────────────────────────────────────┐   │
│  │ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 0%   │   │ ← Progress (empty)
│  └────────────────────────────────────────┘   │
│                                                │
│  [○○○○] [○○○○] [○○○○] [○○○○]                 │ ← All dots empty
│                                                │
│ ┌────────┐  ┌────────┐  ┌────────┐  ┌──────┐ │
│ │ STATS  │  │ START  │  │        │  │ SET  │ │ ← 3 buttons only
│ └────────┘  └────────┘  └────────┘  └──────┘ │
└────────────────────────────────────────────────┘
```

**POMODORO State (Session 1, 18:23 remaining):**
```
┌────────────────────────────────────────────────┐
│ Bat:83% │ WiFi:● │ Mode:BAL │ Time:14:28     │
├────────────────────────────────────────────────┤
│                                                │
│           POMODORO SESSION                     │ ← Session label (red)
│              [Session 1/4]                     │ ← Session counter
│                                                │
│                   18:23                        │ ← Timer (red, large)
│                                                │
│  ┌────────────────────────────────────────┐   │
│  │ ████████████████████░░░░░░░░░░░░░ 63%   │   │ ← Progress (red fill)
│  └────────────────────────────────────────┘   │
│                                                │
│  [●●●○] [○○○○] [○○○○] [○○○○]                 │ ← First dot filled (red)
│                                                │
│ ┌────────┐  ┌────────┐  ┌────────┐  ┌──────┐ │
│ │ STATS  │  │ PAUSE  │  │ STOP   │  │ SET  │ │ ← 4 buttons
│ └────────┘  └────────┘  └────────┘  └──────┘ │
└────────────────────────────────────────────────┘
```

**SHORT_REST State (Session 2 completed, rest 2:34 remaining):**
```
┌────────────────────────────────────────────────┐
│ Bat:82% │ WiFi:● │ Mode:BAL │ Time:14:53     │
├────────────────────────────────────────────────┤
│                                                │
│              SHORT REST                        │ ← Session label (green)
│              [Session 2/4]                     │
│                                                │
│                   02:34                        │ ← Timer (green, large)
│                                                │
│  ┌────────────────────────────────────────┐   │
│  │ █████████████████░░░░░░░░░░░░░░░ 49%    │   │ ← Progress (green fill)
│  └────────────────────────────────────────┘   │
│                                                │
│  [●●●●] [●●●○] [○○○○] [○○○○]                 │ ← 2 Pomodoros complete
│                                                │
│ ┌────────┐  ┌────────┐  ┌────────┐  ┌──────┐ │
│ │ STATS  │  │ PAUSE  │  │ STOP   │  │ SET  │ │
│ └────────┘  └────────┘  └────────┘  └──────┘ │
└────────────────────────────────────────────────┘
```

## 10. Responsive Behavior

### Screen Rotation Lock

M5Stack Core2 is designed for landscape orientation only. No rotation support needed.

### Dynamic Font Scaling (Future Enhancement)

If supporting different display sizes in future:

```cpp
void UIManager::setDisplaySize(uint16_t width, uint16_t height) {
    if (width == 320 && height == 240) {
        FONT_LARGE = &fonts::Orbitron_Light_32;
        FONT_MEDIUM = &fonts::Font4;
    } else if (width == 480 && height == 320) {
        FONT_LARGE = &fonts::Orbitron_Light_48;
        FONT_MEDIUM = &fonts::Font6;
    }
    // Recalculate layouts
}
```

## 11. Error States

### WiFi Disconnected

```
StatusBar: WiFi:○ (empty circle, gray)
Toast (on disconnect): "WiFi disconnected" (warning)
Settings → Cloud: AWS IoT: [DISCONNECTED] ○ (gray)
```

### Cloud Sync Failed

```
Toast: "Cloud sync failed" (error, red)
StatusBar: Add cloud icon with X overlay
Settings → Cloud: Last Sync: Failed 5 min ago (red text)
Action: [RETRY] button appears
```

### Low Memory Warning

```
Toast: "Low memory: Stats sync disabled" (warning)
Settings → System: Free Memory: 45KB / 520KB (red text)
Action: Disable cloud sync temporarily, show restart prompt
```

### Timer Drift Detected

```
// If NTP correction > 60 seconds during active timer
Toast: "Time adjusted by NTP" (info)
Action: Pause timer, show confirmation dialog
Dialog: "System time changed. Continue timer?"
        [YES - Continue] [NO - Reset]
```

## 12. Future Enhancements

### Themes

```cpp
struct Theme {
    uint16_t bg_dark;
    uint16_t bg_card;
    uint16_t primary;
    uint16_t success;
    uint16_t warning;
    uint16_t text_primary;
    uint16_t text_secondary;
};

// Predefined themes
Theme THEME_DEFAULT = { /* current colors */ };
Theme THEME_DARK_BLUE = { 0x0010, 0x1082, 0x05FF, ... };
Theme THEME_FOREST = { 0x0400, 0x1C20, 0x07E0, ... };
```

### Customizable Widgets

Allow users to toggle widget visibility in settings:
- Hide sequence indicator
- Hide progress bar
- Minimal mode (timer only)

### Animated Backgrounds

Subtle particle effects during rest periods (optional, can disable for battery saving):
- Floating circles during rest
- Pulsing gradient during Pomodoro
- Memory cost: ~5KB for particle system

### Gesture Customization

Allow users to remap gyro gestures:
- Rotation angle threshold (30°-90°)
- Flat detection sensitivity
- Double-shake intensity

## 13. Testing Checklist

### Visual Testing

- [ ] All screens render correctly at 320×240
- [ ] Text is readable at arm's length (30cm)
- [ ] Colors meet WCAG AA contrast ratio (4.5:1 for normal text)
- [ ] Touch targets are ≥44×44px
- [ ] No text truncation or overflow

### Performance Testing

- [ ] Full screen redraw <50ms
- [ ] Dirty rectangle update <20ms
- [ ] Sprite rendering <10ms
- [ ] Frame rate maintains 10+ FPS during timer
- [ ] No screen flicker or tearing

### Interaction Testing

- [ ] All buttons respond to touch within 100ms
- [ ] Touch registration is accurate (no missed taps)
- [ ] Gyro gestures trigger correctly (±5° tolerance)
- [ ] Screen transitions are smooth (no janky animations)
- [ ] Toast notifications auto-dismiss after 3s

### Edge Cases

- [ ] Battery at 0%: Low battery UI displays
- [ ] WiFi disconnected: Error state visible
- [ ] Timer at 00:00: Transition to next state
- [ ] Long statistics (99 Pomodoros): Text doesn't overflow
- [ ] Rapid button presses: No duplicate actions

## 14. Implementation Priority

**Phase 1: Core UI** (MP-12, MP-13, MP-14)
1. StatusBar widget
2. TimerDisplay widget
3. Main screen layout (STOPPED state)
4. Button widget + touch handling

**Phase 2: Timer UI** (MP-15)
5. ProgressBar widget
6. SequenceIndicator widget
7. All timer states (POMODORO, SHORT_REST, LONG_REST, PAUSED)
8. Toast notifications

**Phase 3: Statistics UI** (MP-26)
9. Statistics screen layout
10. StatsChart widget
11. Daily/weekly view toggle

**Phase 4: Settings UI** (MP-27)
12. Settings screen layout
13. Settings categories (Timer, Power, Display)
14. Cloud sync popup

**Phase 5: Polish** (MP-28, MP-29)
15. Screen transitions
16. Animations (sequence indicator pulse)
17. Error states
18. Accessibility enhancements

## 15. Related Documents

- [02-ARCHITECTURE.md](02-ARCHITECTURE.md) - Rendering pipeline architecture
- [03-STATE-MACHINE.md](03-STATE-MACHINE.md) - Timer states that drive UI changes
- [04-GYRO-CONTROL.md](04-GYRO-CONTROL.md) - Gyro gesture integration with UI
- [05-STATISTICS-SYSTEM.md](05-STATISTICS-SYSTEM.md) - Data for statistics screen
- [06-POWER-MANAGEMENT.md](06-POWER-MANAGEMENT.md) - Power mode UI indicators

## 16. References

- M5Unified Display API: https://github.com/m5stack/M5Unified
- LGFX Sprite Documentation: https://github.com/lovyan03/LovyanGFX
- Material Design Touch Targets: https://material.io/design/usability/accessibility.html
- WCAG 2.1 Contrast Guidelines: https://www.w3.org/WAI/WCAG21/Understanding/contrast-minimum.html
