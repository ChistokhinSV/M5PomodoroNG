#ifndef RENDERER_H
#define RENDERER_H

#include <M5Unified.h>
#include <vector>

/**
 * Rendering engine with double-buffered canvas and dirty rectangle optimization
 *
 * Features:
 * - Off-screen rendering to PSRAM sprite (320×240, 150KB)
 * - Dirty rectangle tracking for efficient partial updates
 * - Drawing primitives (rect, string, line, circle, bitmap)
 * - Sprite caching for repeated graphics
 * - Target: 30+ FPS for smooth UI updates
 *
 * M5Stack Core2 Display:
 * - Resolution: 320×240 pixels
 * - Color: 16-bit RGB565 (2 bytes per pixel)
 * - Total buffer: 320 × 240 × 2 = 153,600 bytes (~150KB)
 * - PSRAM available: 8MB (plenty for canvas)
 */
class Renderer {
public:
    struct Rect {
        int16_t x;
        int16_t y;
        int16_t w;
        int16_t h;

        bool intersects(const Rect& other) const;
        bool contains(int16_t px, int16_t py) const;
        void merge(const Rect& other);
    };

    struct Color {
        uint16_t rgb565;  // 16-bit RGB565 color

        // Helper constructors
        Color(uint16_t color) : rgb565(color) {}
        Color(uint8_t r, uint8_t g, uint8_t b);

        operator uint16_t() const { return rgb565; }
    };

    Renderer();
    ~Renderer();

    // Initialization
    bool begin();

    // Canvas management
    void clear(Color color = Color(TFT_BLACK));
    void update();  // Push dirty regions to display

    // Dirty rectangle management
    void markDirty(int16_t x, int16_t y, int16_t w, int16_t h);
    void markFullScreenDirty();
    void clearDirty();
    bool isDirty() const { return !dirty_rects.empty(); }

    // Drawing primitives
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, Color color, bool filled = false);
    void drawString(int16_t x, int16_t y, const char* text, const lgfx::IFont* font, Color color);
    void drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, Color color);
    void drawCircle(int16_t x, int16_t y, int16_t radius, Color color, bool filled = false);
    void drawTriangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3, Color color, bool filled = false);

    // Text utilities
    void setTextDatum(textdatum_t datum);
    void setTextSize(float size);
    int16_t textWidth(const char* text, const lgfx::IFont* font = nullptr);
    int16_t fontHeight(const lgfx::IFont* font = nullptr);

    // Performance metrics
    float getFPS() const { return current_fps; }
    uint32_t getLastUpdateMs() const { return last_update_duration_ms; }

    // Constants
    static constexpr int16_t SCREEN_WIDTH = 320;
    static constexpr int16_t SCREEN_HEIGHT = 240;

private:
    LGFX_Sprite canvas;
    std::vector<Rect> dirty_rects;

    // Performance tracking
    uint32_t last_update_time = 0;
    uint32_t last_update_duration_ms = 0;
    uint32_t frame_count = 0;
    float current_fps = 0.0f;

    // Dirty rectangle optimization
    static constexpr uint8_t MAX_DIRTY_RECTS = 10;
    static constexpr float FULL_SCREEN_THRESHOLD = 0.5f;  // 50% coverage = full refresh

    // Helper methods
    void optimizeDirtyRects();
    bool shouldFullRefresh() const;
    void pushDirtyRegions();
};

#endif // RENDERER_H
