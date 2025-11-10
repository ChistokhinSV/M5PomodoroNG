/*
 * M5 Pomodoro Timer v2 - ScreenManager Test
 *
 * Testing navigation between MainScreen, StatsScreen, SettingsScreen, and PauseScreen
 */

#include <M5Unified.h>
#include "ui/Renderer.h"
#include "ui/ScreenManager.h"
#include "core/Config.h"
#include "core/NetworkConfig.h"
#include "core/TimerStateMachine.h"
#include "core/PomodoroSequence.h"
#include "core/Statistics.h"
#include "core/SyncPrimitives.h"
#include "hardware/SDManager.h"
#include "hardware/ILEDController.h"
#include "hardware/LEDController.h"
#include "hardware/IAudioPlayer.h"
#include "hardware/AudioPlayer.h"
#include "hardware/IHapticController.h"
#include "hardware/HapticController.h"
#include "tasks/UITask.h"
#include "tasks/NetworkTask.h"

// ============================================================================
// Global Pointers (accessed by FreeRTOS tasks)
// ============================================================================

// Hardware components
SDManager* g_sdManager = nullptr;

// Network configuration (loaded from SD card)
NetworkConfig* g_networkConfig = nullptr;

// UI components (accessed by UITask on Core 0)
Renderer* g_renderer = nullptr;
ScreenManager* g_screenManager = nullptr;
IAudioPlayer* g_audioPlayer = nullptr;

// Core components (accessed by both cores)
TimerStateMachine* g_stateMachine = nullptr;
PomodoroSequence* g_sequence = nullptr;
Statistics* g_statistics = nullptr;
Config* g_config = nullptr;
ILEDController* g_ledController = nullptr;
IHapticController* g_hapticController = nullptr;

// FreeRTOS task handles (for monitoring)
TaskHandle_t g_uiTaskHandle = NULL;
extern TaskHandle_t g_networkTaskHandle;  // Defined in NetworkTask.cpp

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

    // Initialize FreeRTOS synchronization primitives (for dual-core architecture)
    if (!initSyncPrimitives()) {
        Serial.println("[ERROR] Failed to initialize synchronization primitives");
        while (1) delay(100);  // Fatal error - cannot continue without sync objects
    }
    Serial.println("[OK] FreeRTOS synchronization primitives initialized");

    // Initialize SD card (MP-70)
    g_sdManager = new SDManager();
    if (g_sdManager->begin()) {
        Serial.printf("[OK] SD card initialized: %s, %u MB free\n",
                     g_sdManager->getCardType().c_str(),
                     g_sdManager->getFreeMB());
    } else {
        Serial.println("[WARN] SD card not available - using NVS/FLASH fallbacks");
    }

    // Initialize network configuration from SD card (MP-73)
    if (g_sdManager->isMounted()) {
        g_networkConfig = new NetworkConfig(*g_sdManager);
        if (g_networkConfig->load()) {
            Serial.println("[OK] Network configuration loaded from SD");

            // Try to load SSL certificates (MP-74)
            if (g_networkConfig->loadCertificates()) {
                Serial.println("[OK] SSL certificates loaded");
            } else {
                Serial.println("[WARN] SSL certificates not available - TLS connections disabled");
            }
        } else {
            Serial.println("[WARN] Failed to load network.ini - cloud sync disabled");
            Serial.println("[INFO] Copy config/network.ini.template to SD:/config/network.ini");
        }
    } else {
        Serial.println("[INFO] SD card not available - operating in offline mode");
    }

    // Allocate core components
    g_renderer = new Renderer();
    g_config = new Config();
    g_statistics = new Statistics();
    g_ledController = new LEDController();
    g_hapticController = new HapticController(*g_config);
    g_audioPlayer = new AudioPlayer();
    g_sequence = new PomodoroSequence();
    g_stateMachine = new TimerStateMachine(*g_sequence);

    // Initialize renderer
    if (!g_renderer->begin()) {
        Serial.println("[ERROR] Failed to initialize renderer");
        while (1) delay(100);
    }
    Serial.println("[OK] Renderer initialized");

    // Initialize config
    if (!g_config->begin()) {
        Serial.println("[ERROR] Failed to initialize config");
    } else {
        Serial.println("[OK] Config initialized");
    }

    // Initialize statistics
    if (!g_statistics->begin()) {
        Serial.println("[ERROR] Failed to initialize statistics");
    } else {
        Serial.println("[OK] Statistics initialized");
    }

    // Initialize LED controller
    if (!g_ledController->begin()) {
        Serial.println("[ERROR] Failed to initialize LED controller");
    } else {
        Serial.println("[OK] LED controller initialized");
    }

    // Initialize haptic controller (MP-27)
    if (!g_hapticController->begin()) {
        Serial.println("[ERROR] Failed to initialize haptic controller");
    } else {
        Serial.println("[OK] Haptic controller initialized");
    }

    // Initialize audio player (MP-71: SD audio with FLASH fallback)
    AudioPlayer* audioPlayer = static_cast<AudioPlayer*>(g_audioPlayer);
    if (!audioPlayer->begin(g_sdManager, AudioPlayer::AudioSource::AUTO)) {
        Serial.println("[ERROR] Failed to initialize audio player");
    } else {
        Serial.println("[OK] Audio player initialized");

        // Configure volume and mute from config
        auto ui_config = g_config->getUI();
        g_audioPlayer->setVolume(ui_config.sound_volume);
        if (!ui_config.sound_enabled) {
            g_audioPlayer->mute();
        }
    }

    // Initialize state machine and sequence
    auto pomodoro_config = g_config->getPomodoro();

    // Apply pomodoro settings to sequence
    Serial.printf("[Config] Pomodoro: %d/%d/%d min, %d sessions/cycle, %d cycles\n",
                 pomodoro_config.work_duration_min,
                 pomodoro_config.short_break_min,
                 pomodoro_config.long_break_min,
                 pomodoro_config.sessions_before_long,
                 pomodoro_config.num_cycles);

    g_sequence->setSessionsBeforeLong(pomodoro_config.sessions_before_long);
    g_sequence->setNumCycles(pomodoro_config.num_cycles);
    g_sequence->setWorkDuration(pomodoro_config.work_duration_min);
    g_sequence->setShortBreakDuration(pomodoro_config.short_break_min);
    g_sequence->setLongBreakDuration(pomodoro_config.long_break_min);
    Serial.println("[OK] State machine and sequence initialized");

    // Register audio callback for state machine events
    g_stateMachine->onAudioEvent([](const char* sound_name) {
        if (strcmp(sound_name, "work_start") == 0) {
            g_audioPlayer->play(IAudioPlayer::Sound::WORK_START);
        } else if (strcmp(sound_name, "rest_start") == 0) {
            g_audioPlayer->play(IAudioPlayer::Sound::REST_START);
        } else if (strcmp(sound_name, "long_rest_start") == 0) {
            g_audioPlayer->play(IAudioPlayer::Sound::LONG_REST_START);
        } else if (strcmp(sound_name, "warning") == 0) {
            g_audioPlayer->play(IAudioPlayer::Sound::WARNING);
        }
    });
    Serial.println("[OK] Audio callbacks registered");

    // Connect LED controller to state machine (MP-23)
    g_stateMachine->setLEDController(g_ledController);
    Serial.println("[OK] LED controller connected to state machine");

    // Connect haptic controller to state machine (MP-27)
    g_stateMachine->setHapticController(g_hapticController);
    Serial.println("[OK] Haptic controller connected to state machine");

    // Register timeout callback for auto-start logic
    g_stateMachine->onTimeout([]() {
        // After session completes and sequence advances, check if we should auto-start
        auto session = g_sequence->getCurrentSession();
        auto pomodoro_settings = g_config->getPomodoro();

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
            g_stateMachine->handleEvent(TimerStateMachine::Event::START);
        } else {
            Serial.printf("[Main] Session ready: %s (manual start required)\n",
                         session.type == PomodoroSequence::SessionType::WORK ? "WORK" : "BREAK");
            // MP-23: Show yellow flash to indicate session is waiting for confirmation
            g_stateMachine->indicateSessionReady();
        }
    });
    Serial.println("[OK] Timeout callback registered (auto-start support)");

    // Create ScreenManager (owns all 4 screens)
    g_screenManager = new ScreenManager(*g_stateMachine, *g_sequence, *g_statistics, *g_config, *g_ledController, *g_hapticController);
    Serial.println("[OK] ScreenManager initialized with 4 screens");

    // Note: M5Unified BtnA/B/C zones are fixed at y=240-320, cannot be adjusted
    // On-screen labels at y=218-240 are visual indicators only, actual touch zones are below

    // Update initial status
    uint8_t battery = M5.Power.getBatteryLevel();
    bool charging = M5.Power.isCharging();
    g_screenManager->updateStatus(battery, charging, false, "IDLE", 10, 45);

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

    // ========================================================================
    // Create FreeRTOS Tasks (Dual-core Architecture)
    // ========================================================================

    Serial.println("=== Creating FreeRTOS Tasks ===");

    // Create UI Task on Core 0 (Protocol CPU)
    BaseType_t ui_result = xTaskCreatePinnedToCore(
        uiTask,          // Task function
        "ui_task",       // Task name (for debugging)
        8192,            // Stack size (8KB)
        NULL,            // Parameters (none)
        1,               // Priority (default)
        &g_uiTaskHandle, // Task handle (for monitoring)
        0                // Core 0 (PRO_CPU - Protocol CPU)
    );

    if (ui_result != pdPASS) {
        Serial.println("[ERROR] Failed to create UI task on Core 0");
        while (1) delay(100);
    }
    Serial.println("[OK] UI task created on Core 0 (8KB stack, priority 1)");

    // Create Network Task on Core 1 (Application CPU)
    BaseType_t net_result = xTaskCreatePinnedToCore(
        networkTask,           // Task function
        "network_task",        // Task name (for debugging)
        10240,                 // Stack size (10KB for TLS)
        NULL,                  // Parameters (none)
        1,                     // Priority (same as UI)
        &g_networkTaskHandle,  // Task handle (for monitoring)
        1                      // Core 1 (APP_CPU - Application CPU)
    );

    if (net_result != pdPASS) {
        Serial.println("[ERROR] Failed to create network task on Core 1");
        while (1) delay(100);
    }
    Serial.println("[OK] Network task created on Core 1 (10KB stack, priority 1)");

    Serial.println("\n=== Multi-core Architecture Active ===");
    Serial.println("Core 0: UI, input, state machine, audio, LEDs");
    Serial.println("Core 1: Network (WiFi, MQTT, NTP) - skeleton in Phase 2");
    Serial.println("======================================\n");
}

void loop() {
    // ========================================================================
    // Empty Loop - All Logic Moved to FreeRTOS Tasks
    // ========================================================================
    //
    // The Arduino loop() function is no longer used. All application logic
    // has been moved to FreeRTOS tasks:
    //
    // - uiTask (Core 0): Handles UI, input, rendering, audio, state machine
    // - networkTask (Core 1): Handles WiFi, MQTT, NTP, cloud sync
    //
    // This loop() is kept for compatibility with Arduino framework but does
    // nothing except yield to FreeRTOS scheduler.
    //
    // Note: Arduino setup()/loop() runs on Core 1 by default. Once we create
    // tasks, setup() returns and loop() runs on Core 1, but it's idle since
    // all work is done by our explicit tasks.
    // ========================================================================

    vTaskDelay(pdMS_TO_TICKS(1000));  // Sleep 1 second, yield to tasks
}
