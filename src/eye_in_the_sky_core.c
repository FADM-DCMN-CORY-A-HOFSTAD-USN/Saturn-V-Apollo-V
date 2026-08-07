#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#define CCTV_VIDEO_PORT_INDEX   1  // Port 2 (/dev/ttyS1) - Ingests optical scan matrix streams
#define WEATHER_SENSOR_PORT_IDX 4  // Port 5 (/dev/ttyS4) - Reads humidity and water sensor values

typedef struct {
    float ambient_humidity_percentage;
    bool liquid_water_detected_on_lens;
    float movement_intensity_gradient;
    bool motion_alarm_tripped;
    bool weather_anomaly_flag;
} EyeInTheSkyHardwareStack;

EyeInTheSkyHardwareStack cctv_core = { 45.2f, false, 0.0f, false, false };

/**
 * Analyzes frame telemetry over the video bus. Computes delta changes between 
 * sequential frame buffers to detect target tracking movements.
 */
void process_optical_motion_matrix(int video_file_descriptor) {
    char frame_buffer[32];
    int bytes_read = read(video_file_descriptor, frame_buffer, sizeof(frame_buffer) - 1);
    
    if (bytes_read > 0) {
        frame_buffer[bytes_read] = '\0';
        // Parse numerical optical delta change output from the camera board
        float current_frame_delta = atof(frame_buffer);
        cctv_core.movement_intensity_gradient = current_frame_delta;

        // MOTION THRESHOLD INTERLOCK: Trigger alarm if frame variance exceeds 15%
        const float MAXIMUM_MOTION_ALLOWED_DELTA = 15.0f;
        if (cctv_core.movement_intensity_gradient >= MAXIMUM_MOTION_ALLOWED_DELTA) {
            cctv_core.motion_alarm_tripped = true;
        } else {
            cctv_core.motion_alarm_tripped = false;
        }
    }
}

/**
 * Processes weather diagnostics over Port 5. Monitors lens dew-point 
 * humidity vectors and flags liquid condensation build-ups.
 */
void evaluate_weather_envelope(int weather_file_descriptor) {
    char diagnostic_buffer[64];
    int bytes_read = read(weather_file_descriptor, diagnostic_buffer, sizeof(diagnostic_buffer) - 1);
    
    if (bytes_read > 0) {
        diagnostic_buffer[bytes_read] = '\0';
        
        // Expected incoming packet string schema: "HUMIDITY=78.5;WATER=1"
        float parsed_humidity = 0.0f;
        int parsed_water_flag = 0;
        
        if (sscanf(diagnostic_buffer, "HUMIDITY=%f;WATER=%d", &parsed_humidity, &parsed_water_flag) >= 2) {
            cctv_core.ambient_humidity_percentage = parsed_humidity;
            cctv_core.liquid_water_detected_on_lens = (parsed_water_flag == 1);
        }
    }

    // WEATHER CHANGE ALARM INDICES: >85% Humidity or direct liquid water contact
    if (cctv_core.ambient_humidity_percentage >= 85.0f || cctv_core.liquid_water_detected_on_lens) {
        cctv_core.weather_anomaly_flag = true;
    } else {
        cctv_core.weather_anomaly_flag = false;
    }
}

/**
 * Orchestrates the full surveillance pass loop. If changes are detected,
 * it outputs track notifications directly down the Athena pipeline.
 */
void execute_surveillance_pass(int* serial_file_descriptors) {
    int v_fd = serial_file_descriptors[CCTV_VIDEO_PORT_INDEX];
    int w_fd = serial_file_descriptors[WEATHER_SENSOR_PORT_IDX];

    // 1. Refresh active sensor input structures
    process_optical_motion_matrix(v_fd);
    evaluate_weather_envelope(w_fd);

    // 2. Evaluate alarms against guidance handshakes
    if (cctv_core.motion_alarm_tripped || cctv_core.weather_anomaly_flag) {
        printf("\n=================================================================\n");
        printf("[EYE IN THE SKY: SCAN RELAY INTERCEPT TRIPPED]\n");
        printf("-> OPTICAL MOTION ACCELERATION GRADIENT: %.2f%%\n", cctv_core.movement_intensity_gradient);
        printf("-> ATMOSPHERIC CONDITIONS: Humidity = %.1f%% | Liquid Water Contact: %s\n", 
               cctv_core.ambient_humidity_percentage, cctv_core.liquid_water_detected_on_lens ? "YES (CONDENSATION)" : "NO");
        printf("-> ACTION: Injecting target observation data directly to Athena.\n");
        printf("-> STATUS: Saturated cockpit RCS alert lines over port 8080.\n");
        printf("=================================================================\n");
    }
}
