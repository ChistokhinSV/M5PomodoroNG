/*
 * M5 Pomodoro Timer v2 - Renderer Test
 *
 * Testing Renderer with visual patterns and performance metrics
 */

#include <M5Unified.h>
#include "ui/Renderer.h"

Renderer renderer;
uint32_t lastUpdate = 0;
uint8_t testMode = 0;

void drawTestPattern1() {
    renderer.clear(Renderer::Color(TFT_BLACK));

    // Draw rectangles
    renderer.drawRect(10, 10, 100, 50, Renderer::Color(TFT_RED), true);
    renderer.drawRect(120, 10, 100, 50, Renderer::Color(TFT_GREEN), false);
    renderer.drawRect(230, 10, 80, 50, Renderer::Color(TFT_BLUE), true);

    // Draw text
    renderer.setTextDatum(MC_DATUM);
    renderer.drawString(160, 80, "Renderer Test", &fonts::Font4, Renderer::Color(TFT_WHITE));
    renderer.drawString(160, 110, "Pattern 1: Shapes", &fonts::Font2, Renderer::Color(TFT_CYAN));

    // Draw circles
    renderer.drawCircle(60, 160, 30, Renderer::Color(TFT_YELLOW), false);
    renderer.drawCircle(160, 160, 30, Renderer::Color(TFT_MAGENTA), true);
    renderer.drawCircle(260, 160, 30, Renderer::Color(TFT_ORANGE), false);

    // Draw lines
    renderer.drawLine(10, 210, 100, 230, Renderer::Color(TFT_WHITE));
    renderer.drawLine(120, 210, 210, 230, Renderer::Color(TFT_CYAN));
    renderer.drawLine(220, 210, 310, 230, Renderer::Color(TFT_GREEN));
}

void drawTestPattern2() {
    renderer.clear(Renderer::Color(TFT_NAVY));

    // Draw title
    renderer.setTextDatum(MC_DATUM);
    renderer.drawString(160, 30, "Pattern 2: Animation", &fonts::Font4, Renderer::Color(TFT_WHITE));

    // Animated bouncing rectangle
    static int16_t x = 50;
    static int16_t y = 80;
    static int8_t dx = 2;
    static int8_t dy = 2;

    const int16_t boxSize = 40;

    // Update position
    x += dx;
    y += dy;

    // Bounce off edges
    if (x < 0 || x + boxSize > 320) dx = -dx;
    if (y < 50 || y + boxSize > 200) dy = -dy;

    // Draw box
    renderer.drawRect(x, y, boxSize, boxSize, Renderer::Color(TFT_YELLOW), true);

    // Draw performance info
    char fpsStr[32];
    snprintf(fpsStr, sizeof(fpsStr), "FPS: %.1f", renderer.getFPS());
    renderer.drawString(160, 220, fpsStr, &fonts::Font2, Renderer::Color(TFT_GREEN));
}

void drawTestPattern3() {
    // Partial update test - only redraw timer
    static uint8_t counter = 0;
    counter++;

    if (counter % 10 == 0) {
        // Clear center region only
        renderer.drawRect(60, 80, 200, 80, Renderer::Color(TFT_BLACK), true);

        // Draw updated timer
        char timeStr[16];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", (counter / 10) / 60, (counter / 10) % 60);
        renderer.setTextDatum(MC_DATUM);
        renderer.drawString(160, 120, timeStr, &fonts::Font6, Renderer::Color(TFT_CYAN));

        // Draw subtitle
        renderer.drawString(160, 160, "Dirty Rect Optimization", &fonts::Font2, Renderer::Color(TFT_WHITE));

        char updateStr[32];
        snprintf(updateStr, sizeof(updateStr), "Update: %lu ms", renderer.getLastUpdateMs());
        renderer.drawString(160, 220, updateStr, &fonts::Font2, Renderer::Color(TFT_GREEN));
    }
}

void setup() {
    // Initialize Serial
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n=================================");
    Serial.println("M5 Pomodoro v2 - Renderer Test");
    Serial.println("=================================");

    // Initialize M5Unified
    auto cfg = M5.config();
    cfg.clear_display = true;
    cfg.internal_imu = false;
    cfg.internal_rtc = false;
    cfg.internal_spk = false;
    cfg.internal_mic = false;
    M5.begin(cfg);

    Serial.println("[OK] M5Unified initialized");

    // Initialize Renderer
    if (!renderer.begin()) {
        Serial.println("[ERROR] Renderer initialization failed!");
        while (1) delay(100);
    }

    Serial.println("[OK] Renderer initialized");
    Serial.println("\nTest Controls:");
    Serial.println("- Touch screen to cycle test patterns");
    Serial.println("- Pattern 1: Static shapes");
    Serial.println("- Pattern 2: Animation test");
    Serial.println("- Pattern 3: Dirty rect optimization\n");

    // Draw initial pattern
    drawTestPattern1();
    renderer.update();

    lastUpdate = millis();
}

void loop() {
    M5.update();

    // Check for touch to cycle test modes
    auto touch = M5.Touch.getDetail();
    if (touch.wasPressed()) {
        testMode = (testMode + 1) % 3;
        Serial.printf("[Test] Switching to pattern %d\n", testMode + 1);

        // Force full redraw on mode change
        renderer.clear(Renderer::Color(TFT_BLACK));
    }

    // Update at 30 FPS (~33ms per frame)
    uint32_t now = millis();
    if (now - lastUpdate >= 33) {
        lastUpdate = now;

        // Draw current test pattern
        switch (testMode) {
            case 0:
                // Static pattern (draw once, no updates needed)
                if (touch.wasPressed()) {
                    drawTestPattern1();
                }
                break;

            case 1:
                drawTestPattern2();  // Animated
                break;

            case 2:
                drawTestPattern3();  // Partial updates
                break;
        }

        // Push updates to screen
        renderer.update();
    }

    delay(1);  // Prevent watchdog
}
