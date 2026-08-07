#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>

#define THERMOMETER_PORT_INDEX 4  // Port 5 (/dev/ttyS4) - Reads Ambient Telemetry
#define COOLING_FAN_PORT_INDEX  5  // Port 6 (/dev/ttyS5) - Controls Fan Relay Drivers

// Core Environmental State Registers
typedef struct {
    float internal_capsule_temperature_f;
    bool cooling_fans_active;
    unsigned int active_fan_speed_rpm;
} CapsuleEnvironmentalCore;

CapsuleEnvironmentalCore ecls_status = { 68.5f, false, 0 };

/**
 * High-priority thermal evaluation routine.
 * Triggers all cooling fans at maximum output when temperature thresholds are breached.
 */
void evaluate_capsule_thermal_envelope(int* serial_file_descriptors) {
    char read_buffer[32];
    int therm_fd = serial_file_descriptors[THERMOMETER_PORT_INDEX];
    int fan_fd = serial_file_descriptors[COOLING_FAN_PORT_INDEX];
    
    // Read raw data from the onboard thermometer over Port 5
    int bytes_read = read(therm_fd, read_buffer, sizeof(read_buffer) - 1);
    if (bytes_read > 0) {
        read_buffer[bytes_read] = '\0';
        // Convert ASCII sensor data package directly to float
        ecls_status.internal_capsule_temperature_f = atof(read_buffer);
    }

    // MANDATORY CRITICAL TEMPERATURE INTERLOCK: 75 Degrees Fahrenheit
    const float CRITICAL_COOLING_THRESHOLD_F = 75.0f;
    
    if (ecls_status.internal_capsule_temperature_f >= CRITICAL_COOLING_THRESHOLD_F) {
        // Force emergency cooling state changes
        ecls_status.cooling_fans_active = true;
        ecls_status.active_fan_speed_rpm = 4500; // Force full high-output mechanical velocity
        
        // Assert a permanent high voltage signal down Port 6 to kick open physical fan relays
        write(fan_fd, "FAN_ARRAY_MAX_BUS_HIGH\n", 23);
        
        printf("\n=================================================================\n");
        printf("[THERMAL WARNING] Capsule sensor indicates limit breach: %.2f°F\n", ecls_status.internal_capsule_temperature_f);
        printf("-> LIMIT CONDITION: Exceeded critical threshold (>= %.1f°F).\n", CRITICAL_COOLING_THRESHOLD_F);
        printf("-> ACTION: Forcing all auxiliary cooling fans to MAX OUTPUT.\n");
        printf("-> STATUS: Port 6 relay bus saturated at +5.0V. Fans spinning at %d RPM.\n", ecls_status.active_fan_speed_rpm);
        printf("=================================================================\n");
    } else {
        // If temperature sits in a healthy zone, keep fan currents at tracking defaults
        if (ecls_status.cooling_fans_active && ecls_status.internal_capsule_temperature_f < 72.0f) {
            // Apply a 3-degree deadband buffer to prevent short-cycling the fan motors
            ecls_status.cooling_fans_active = false;
            ecls_status.active_fan_speed_rpm = 0;
            write(fan_fd, "FAN_ARRAY_STANDBY\n", 18);
            printf("[THERMAL INFO] Temperature stabilized. Returning fans to standby loops.\n");
        }
    }
}
