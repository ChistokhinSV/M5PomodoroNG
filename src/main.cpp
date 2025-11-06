/*
 * M5 Pomodoro Timer v2 - ScreenManager Test
 *
 * Testing navigation between MainScreen, StatsScreen, SettingsScreen, and PauseScreen
 */

#include <M5Unified.h>
#include "ui/Renderer.h"
#include "ui/ScreenManager.h"
#include "core/Config.h"
#include "core/TimerStateMachine.h"
#include "core/PomodoroSequence.h"
#include "core/Statistics.h"
#include "hardware/LEDController.h"
#include "hardware/AudioPlayer.h"

Renderer renderer;
uint32_t lastUpdate = 0;
uint32_t lastSecond = 0;

// Core components
Config config;
PomodoroSequence sequence;
TimerStateMachine stateMachine(sequence);  // Needs sequence reference
Statistics statistics;
LEDController ledController;
AudioPlayer audioPlayer;

// Screen Manager (manages all screens and navigation)
ScreenManager* screenManager = nullptr;

void setup() {
    // Initialize M5
    auto cfg = M5.config();
    M5.begin(cfg);

    Serial.begin(115200);
    delay(100);

    Serial.println("\n=================================");
    Serial.println("M5 Pomodoro v2 - ScreenManager Test");
    Serial.println("=================================");

    // Check PSRAM
    if (psramFound()) {
        size_t psram_size = ESP.getPsramSize();
        size_t free_psram = ESP.getFreePsram();
        Serial.printf("[OK] PSRAM detected: %d bytes total, %d bytes free\n",
                     psram_size, free_psram);
    } else {
        Serial.println("[WARN] No PSRAM detected");
    }

    // Initialize renderer
    if (!renderer.begin()) {
        Serial.println("[ERROR] Failed to initialize renderer");
        while (1) delay(100);
    }
    Serial.println("[OK] Renderer initialized");

    // Initialize config
    if (!config.begin()) {
        Serial.println("[ERROR] Failed to initialize config");
    } else {
        Serial.println("[OK] Config initialized");
    }

    // Initialize statistics
    if (!statistics.begin()) {
        Serial.println("[ERROR] Failed to initialize statistics");
    } else {
        Serial.println("[OK] Statistics initialized");
    }

    // Initialize LED controller
    if (!ledController.begin()) {
        Serial.println("[ERROR] Failed to initialize LED controller");
    } else {
        Serial.println("[OK] LED controller initialized");
    }

    // Initialize audio player
    if (!audioPlayer.begin()) {
        Serial.println("[ERROR] Failed to initialize audio player");
    } else {
        Serial.println("[OK] Audio player initialized");

        // Configure volume and mute from config
        auto ui_config = config.getUI();
        audioPlayer.setVolume(ui_config.sound_volume);
        if (!ui_config.sound_enabled) {
            audioPlayer.mute();
        }
    }

    // Initialize state machine and sequence
    auto pomodoro_config = config.getPomodoro();

    // Detect mode based on config values
    PomodoroSequence::Mode mode = PomodoroSequence::Mode::CLASSIC;
    if (pomodoro_config.sessions_before_long != 4 ||
        pomodoro_config.work_duration_min != 25 ||
        pomodoro_config.short_break_min != 5 ||
        pomodoro_config.long_break_min != 15) {
        mode = PomodoroSequence::Mode::CUSTOM;
        Serial.println("[Config] Using CUSTOM mode (non-default values)");
    } else {
        Serial.println("[Config] Using CLASSIC mode (default values)");
    }

    sequence.setMode(mode);
    sequence.setSessionsBeforeLong(pomodoro_config.sessions_before_long);
    sequence.setWorkDuration(pomodoro_config.work_duration_min);
    sequence.setShortBreakDuration(pomodoro_config.short_break_min);
    sequence.setLongBreakDuration(pomodoro_config.long_break_min);
    Serial.println("[OK] State machine and sequence initialized");

    // Register audio callback for state machine events
    stateMachine.onAudioEvent([](const char* sound_name) {
        if (strcmp(sound_name, "work_start") == 0) {
            audioPlayer.play(AudioPlayer::Sound::WORK_START);
        } else if (strcmp(sound_name, "rest_start") == 0) {
            audioPlayer.play(AudioPlayer::Sound::REST_START);
        } else if (strcmp(sound_name, "long_rest_start") == 0) {
            audioPlayer.play(AudioPlayer::Sound::LONG_REST_START);
        } else if (strcmp(sound_name, "warning") == 0) {
            audioPlayer.play(AudioPlayer::Sound::WARNING);
        }
    });
    Serial.println("[OK] Audio callbacks registered");

    // Register timeout callback for auto-start logic
    stateMachine.onTimeout([&]() {
        // After session completes and sequence advances, check if we should auto-start
        auto session = sequence.getCurrentSession();
        auto pomodoro_settings = config.getPomodoro();

        bool should_auto_start = false;
        if (session.type == PomodoroSequence::SessionType::WORK) {
            should_auto_start = pomodoro_settings.auto_start_work;
        } else {
            // SHORT_BREAK or LONG_BREAK
            should_auto_start = pomodoro_settings.auto_start_breaks;
        }

        if (should_auto_start) {
            Serial.printf("[Main] Auto-starting next session: %s\n",
                         session.type == PomodoroSequence::SessionType::WORK ? "WORK" : "BREAK");
            stateMachine.handleEvent(TimerStateMachine::Event::START);
        } else {
            Serial.printf("[Main] Session ready: %s (manual start required)\n",
                         session.type == PomodoroSequence::SessionType::WORK ? "WORK" : "BREAK");
        }
    });
    Serial.println("[OK] Timeout callback registered (auto-start support)");

    // Create ScreenManager (owns all 4 screens)
    screenManager = new ScreenManager(stateMachine, sequence, statistics, config, ledController);
    Serial.println("[OK] ScreenManager initialized with 4 screens");

    // Note: M5Unified BtnA/B/C zones are fixed at y=240-320, cannot be adjusted
    // On-screen labels at y=218-240 are visual indicators only, actual touch zones are below

    // Update initial status
    uint8_t battery = M5.Power.getBatteryLevel();
    bool charging = M5.Power.isCharging();
    screenManager->updateStatus(battery, charging, false, "IDLE", 10, 45);

    Serial.println("\nControls:");
    Serial.println("- MainScreen: [Start] [Stats] [Set] buttons");
    Serial.println("- StatsScreen: [<- Back] button");
    Serial.println("- SettingsScreen: [<- Back] [Prev] [Next] buttons (5 pages)");
    Serial.println("- PauseScreen: Auto-navigate when timer paused");
    Serial.println("\nNavigation flow:");
    Serial.println("  MainScreen <-> StatsScreen (Stats/Back buttons)");
    Serial.println("  MainScreen <-> SettingsScreen (Set/Back buttons)");
    Serial.println("  MainScreen <-> PauseScreen (auto-managed by state machine)");
    Serial.println("\n[OK] All screens ready\n");

    lastUpdate = millis();
    lastSecond = millis();
}

void loop() {
    M5.update();

    // Handle hardware button presses (BtnA/B/C capacitive touch zones)
    screenManager->handleHardwareButtons();

    // Handle touch events (for widgets like sliders/toggles)
    auto touch = M5.Touch.getDetail();

    if (touch.wasPressed()) {
        screenManager->handleTouch(touch.x, touch.y, true);
    }

    if (touch.wasReleased()) {
        screenManager->handleTouch(touch.x, touch.y, false);
    }

    // Update at 30 FPS (~33ms per frame)
    uint32_t now = millis();
    uint32_t deltaMs = now - lastUpdate;

    if (deltaMs >= 33) {
        lastUpdate = now;

        // Update ScreenManager (which updates active screen)
        screenManager->update(deltaMs);

        // Update audio player (track playing state)
        audioPlayer.update();

        // Draw active screen
        screenManager->draw(renderer);

        // Push to display
        renderer.update();
    }

    // Update status bar every second
    if (now - lastSecond >= 1000) {
        lastSecond = now;

        // Update battery and time (with validation to avoid 0% glitches)
        static uint8_t last_valid_battery = 100;
        uint8_t battery = M5.Power.getBatteryLevel();

        // Validate battery reading (M5.Power can return 0 on I2C glitches)
        if (battery == 0 || battery > 100) {
            battery = last_valid_battery;  // Use last known good value
        } else {
            last_valid_battery = battery;
        }

        bool charging = M5.Power.isCharging();

        // Get current time (use millis for now, will use RTC later)
        static uint8_t hour = 10;
        static uint8_t minute = 45;
        minute++;
        if (minute >= 60) {
            minute = 0;
            hour = (hour + 1) % 24;
        }

        // Get current mode string
        const char* mode = "IDLE";
        auto state = stateMachine.getState();
        if (state == TimerStateMachine::State::ACTIVE) {
            mode = sequence.isWorkSession() ? "WORK" : "BREAK";
        } else if (state == TimerStateMachine::State::PAUSED) {
            mode = "PAUSED";
        }

        screenManager->updateStatus(battery, charging, false, mode, hour, minute);
    }

    delay(1);  // Prevent watchdog
}
