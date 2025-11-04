#include "Renderer.h"
#include <Arduino.h>

// Rect helper methods
bool Renderer::Rect::intersects(const Rect& other) const {
    return !(x + w < other.x || other.x + other.w < x ||
             y + h < other.y || other.y + other.h < y);
}

bool Renderer::Rect::contains(int16_t px, int16_t py) const {
    return (px >= x && px < x + w && py >= y && py < y + h);
}

void Renderer::Rect::merge(const Rect& other) {
    int16_t x2 = max(x + w, other.x + other.w);
    int16_t y2 = max(y + h, other.y + other.h);
    x = min(x, other.x);
    y = min(y, other.y);
    w = x2 - x;
    h = y2 - y;
}

// Color helper
Renderer::Color::Color(uint8_t r, uint8_t g, uint8_t b) {
    // Convert RGB888 to RGB565
    rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Renderer implementation
Renderer::Renderer()
    : last_update_time(0),
      last_update_duration_ms(0),
      frame_count(0),
      current_fps(0.0f) {
}

Renderer::~Renderer() {
    canvas.deleteSprite();
}

bool Renderer::begin() {
    Serial.println("[Renderer] Initializing...");

    // Check PSRAM availability and memory before sprite creation
    Serial.printf("[Renderer] Free heap: %d bytes\n", ESP.getFreeHeap());
    if (psramFound()) {
        Serial.printf("[Renderer] PSRAM found: %d bytes total, %d bytes free\n",
                      ESP.getPsramSize(), ESP.getFreePsram());
    } else {
        Serial.println("[Renderer] WARNING: PSRAM not detected");
    }

    // Configure sprite for PSRAM allocation BEFORE creating it
    canvas.setColorDepth(16);  // RGB565 = 2 bytes per pixel
    canvas.setPsram(true);     // CRITICAL: Must be called before createSprite()

    // Create full-screen sprite (M5GFX will allocate in PSRAM)
    Serial.printf("[Renderer] Creating %dx%d sprite in PSRAM...\n", SCREEN_WIDTH, SCREEN_HEIGHT);
    void* sprite_buffer = canvas.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!sprite_buffer) {
        Serial.println("[Renderer] ERROR: Failed to create canvas sprite");
        Serial.printf("[Renderer] Required: %d bytes\n", SCREEN_WIDTH * SCREEN_HEIGHT * 2);
        Serial.printf("[Renderer] Free heap: %d bytes\n", ESP.getFreeHeap());
        Serial.printf("[Renderer] Free PSRAM: %d bytes\n", ESP.getFreePsram());
        return false;
    }

    Serial.printf("[Renderer] Sprite buffer created at %p\n", sprite_buffer);

    // Verify buffer is in PSRAM (PSRAM addresses: 0x3F800000-0x3FC00000)
    if ((uint32_t)sprite_buffer >= 0x3F800000 && (uint32_t)sprite_buffer < 0x3FC00000) {
        Serial.println("[Renderer] ✓ Buffer successfully allocated in PSRAM");
    } else {
        Serial.printf("[Renderer] WARNING: Buffer at %p is NOT in PSRAM (heap)\n", sprite_buffer);
    }

    // Initialize canvas
    canvas.fillScreen(TFT_BLACK);
    canvas.setTextDatum(MC_DATUM);  // Middle-center
    canvas.setTextSize(1.0f);

    // Mark full screen dirty for initial render
    markFullScreenDirty();

    Serial.printf("[Renderer] Initialized: %dx%d canvas (%d bytes)\n",
                  SCREEN_WIDTH, SCREEN_HEIGHT,
                  SCREEN_WIDTH * SCREEN_HEIGHT * 2);
    Serial.printf("[Renderer] Free heap after init: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("[Renderer] Free PSRAM after init: %d bytes\n", ESP.getFreePsram());

    return true;
}

void Renderer::clear(Color color) {
    canvas.fillScreen(color.rgb565);
    markFullScreenDirty();
}

void Renderer::update() {
    if (!isDirty()) {
        return;  // Nothing to update
    }

    uint32_t start = millis();

    // Optimize dirty rectangles (merge overlapping)
    optimizeDirtyRects();

    // Push to display
    pushDirtyRegions();

    // Clear dirty flags
    clearDirty();

    // Update performance metrics
    last_update_duration_ms = millis() - start;
    frame_count++;

    // Calculate FPS (every second)
    if (millis() - last_update_time >= 1000) {
        current_fps = frame_count * 1000.0f / (millis() - last_update_time);
        frame_count = 0;
        last_update_time = millis();

        // Log FPS periodically
        static uint32_t last_log = 0;
        if (millis() - last_log >= 5000) {
            Serial.printf("[Renderer] FPS: %.1f, Update: %lu ms\n",
                          current_fps, last_update_duration_ms);
            last_log = millis();
        }
    }
}

void Renderer::markDirty(int16_t x, int16_t y, int16_t w, int16_t h) {
    // Clip to screen bounds
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_WIDTH) w = SCREEN_WIDTH - x;
    if (y + h > SCREEN_HEIGHT) h = SCREEN_HEIGHT - y;

    if (w <= 0 || h <= 0) return;  // Invalid rectangle

    Rect rect = {x, y, w, h};

    // Try to merge with existing dirty rectangles
    for (auto& existing : dirty_rects) {
        if (existing.intersects(rect)) {
            existing.merge(rect);
            return;
        }
    }

    // Add as new dirty rectangle
    if (dirty_rects.size() < MAX_DIRTY_RECTS) {
        dirty_rects.push_back(rect);
    } else {
        // Too many dirty rects, mark full screen
        markFullScreenDirty();
    }
}

void Renderer::markFullScreenDirty() {
    dirty_rects.clear();
    dirty_rects.push_back({0, 0, SCREEN_WIDTH, SCREEN_HEIGHT});
}

void Renderer::clearDirty() {
    dirty_rects.clear();
}

void Renderer::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, Color color, bool filled) {
    if (filled) {
        canvas.fillRect(x, y, w, h, color.rgb565);
    } else {
        canvas.drawRect(x, y, w, h, color.rgb565);
    }
    markDirty(x, y, w, h);
}

void Renderer::drawString(int16_t x, int16_t y, const char* text, const lgfx::IFont* font, Color color) {
    if (font) {
        canvas.setFont(font);
    }

    canvas.setTextColor(color.rgb565);
    canvas.drawString(text, x, y);

    // Mark text bounding box as dirty
    int16_t text_w = canvas.textWidth(text);
    int16_t text_h = canvas.fontHeight();
    markDirty(x - text_w / 2, y - text_h / 2, text_w, text_h);
}

void Renderer::drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, Color color) {
    canvas.drawLine(x1, y1, x2, y2, color.rgb565);

    // Mark bounding box of line as dirty
    int16_t min_x = min(x1, x2);
    int16_t min_y = min(y1, y2);
    int16_t max_x = max(x1, x2);
    int16_t max_y = max(y1, y2);
    markDirty(min_x, min_y, max_x - min_x + 1, max_y - min_y + 1);
}

void Renderer::drawCircle(int16_t x, int16_t y, int16_t radius, Color color, bool filled) {
    if (filled) {
        canvas.fillCircle(x, y, radius, color.rgb565);
    } else {
        canvas.drawCircle(x, y, radius, color.rgb565);
    }
    markDirty(x - radius, y - radius, radius * 2, radius * 2);
}

void Renderer::drawTriangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                            int16_t x3, int16_t y3, Color color, bool filled) {
    if (filled) {
        canvas.fillTriangle(x1, y1, x2, y2, x3, y3, color.rgb565);
    } else {
        canvas.drawTriangle(x1, y1, x2, y2, x3, y3, color.rgb565);
    }

    // Mark bounding box of triangle as dirty
    int16_t min_x = min(x1, min(x2, x3));
    int16_t min_y = min(y1, min(y2, y3));
    int16_t max_x = max(x1, max(x2, x3));
    int16_t max_y = max(y1, max(y2, y3));
    markDirty(min_x, min_y, max_x - min_x + 1, max_y - min_y + 1);
}

void Renderer::setTextDatum(textdatum_t datum) {
    canvas.setTextDatum(datum);
}

void Renderer::setTextSize(float size) {
    canvas.setTextSize(size);
}

int16_t Renderer::textWidth(const char* text, const lgfx::IFont* font) {
    if (font) {
        canvas.setFont(font);
    }
    return canvas.textWidth(text);
}

int16_t Renderer::fontHeight(const lgfx::IFont* font) {
    if (font) {
        canvas.setFont(font);
    }
    return canvas.fontHeight();
}

// Private methods

void Renderer::optimizeDirtyRects() {
    if (dirty_rects.size() <= 1) {
        return;  // Nothing to optimize
    }

    // Simple optimization: merge overlapping rectangles
    // (More sophisticated algorithms possible, but this is sufficient)
    bool merged = true;
    while (merged && dirty_rects.size() > 1) {
        merged = false;
        for (size_t i = 0; i < dirty_rects.size(); i++) {
            for (size_t j = i + 1; j < dirty_rects.size(); j++) {
                if (dirty_rects[i].intersects(dirty_rects[j])) {
                    dirty_rects[i].merge(dirty_rects[j]);
                    dirty_rects.erase(dirty_rects.begin() + j);
                    merged = true;
                    break;
                }
            }
            if (merged) break;
        }
    }
}

bool Renderer::shouldFullRefresh() const {
    if (dirty_rects.size() == 1 &&
        dirty_rects[0].w == SCREEN_WIDTH &&
        dirty_rects[0].h == SCREEN_HEIGHT) {
        return true;  // Already marked for full refresh
    }

    // Calculate total dirty area
    uint32_t total_dirty_area = 0;
    for (const auto& rect : dirty_rects) {
        total_dirty_area += rect.w * rect.h;
    }

    uint32_t screen_area = SCREEN_WIDTH * SCREEN_HEIGHT;
    return (total_dirty_area > screen_area * FULL_SCREEN_THRESHOLD);
}

void Renderer::pushDirtyRegions() {
    // Note: M5GFX doesn't support partial sprite updates via pushSprite()
    // For now, always push full sprite (still benefits from dirty rect tracking
    // to avoid unnecessary canvas draws)
    // TODO: Optimize with setAddrWindow() + writePixels() for true partial updates
    canvas.pushSprite(&M5.Display, 0, 0);
}
