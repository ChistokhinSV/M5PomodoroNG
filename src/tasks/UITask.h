#ifndef UI_TASK_H
#define UI_TASK_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/**
 * UI Task function for Core 0 (PRO_CPU)
 *
 * This task replaces the Arduino loop() and runs continuously on Core 0.
 * It handles all UI-related operations: touch input, rendering, audio playback.
 *
 * Usage:
 *   xTaskCreatePinnedToCore(
 *       uiTask,          // Task function
 *       "ui_task",       // Task name
 *       8192,            // Stack size (8KB)
 *       NULL,            // Parameters
 *       1,               // Priority (default)
 *       NULL,            // Task handle
 *       0                // Core 0 (PRO_CPU)
 *   );
 *
 * @param parameter Task parameter (not used, pass NULL)
 */
void uiTask(void* parameter);

#endif // UI_TASK_H
