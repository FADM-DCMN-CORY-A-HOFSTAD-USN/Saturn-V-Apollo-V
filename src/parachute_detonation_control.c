#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define MASTER_COMMAND_RADIO_PORT  2  // Port 3 (/dev/ttyS2) - Receives over-the-horizon remote commands
#define PAYLOAD_TELEMETRY_BUS_PORT 1  // Port 2 (/dev/ttyS1) - Monitors gyro drift and landing parachute states

typedef struct {
    bool capsule_parachute_deployed;
    bool environmental_safeties_bypassed;
    bool remote_detonation_bus_armed;
    bool payload_vaporized;
} ParachuteRecoveryHardwareStack;

ParachuteRecoveryHardwareStack recovery_matrix = { false, false, false, false };

/**
 * Handles terminal recovery tracking. Once the parachute is deployed, it bypasses
 * standard safety geofences to allow instant remote detonation via the command radio.
 */
void process_parachute_recovery_telemetry(int* serial_file_descriptors, bool live_chute_sensor_flag) {
    int radio_fd = serial_file_descriptors[MASTER_COMMAND_RADIO_PORT];
    int telemetry_fd = serial_file_descriptors[PAYLOAD_TELEMETRY_BUS_PORT];

    recovery_matrix.capsule_parachute_deployed = live_chute_sensor_flag;

    // MANDATORY PARACHUTE DEPLOYMENT OVERRIDE
    if (recovery_matrix.capsule_parachute_deployed) {
        // Force complete bypass of environmental pressure ceilings and friendly airspace boundaries
        recovery_matrix.environmental_safeties_bypassed = true;
        recovery_matrix.remote_detonation_bus_armed = true;

        printf("\n=================================================================\n");
        printf("[RECOVERY INTERLOCK ENGAGED] Parachute deployment verified via tracking.\n");
        printf("-> RETRIEVAL MODE: OVERRIDING ALL GEOPOLITICAL AND AIRSPACE INHIBITS.\n");
        printf("-> ACTION: Activating Port 3 Command Radio listening arrays.\n");
        printf("-> STATUS: Remote-ready detonation bus ARMED. Awaiting remote packet.\n");
        printf("=================================================================\n");

        // Continually poll Port 3 (/dev/ttyS2) for the specialized remote detonation packet string
        char radio_buffer[64];
        int bytes_read = read(radio_fd, radio_buffer, sizeof(radio_buffer) - 1);
        
        if (bytes_read > 0) {
            radio_buffer[bytes_read] = '\0';
            if (strstr(radio_buffer, "REMOTE_SECURE_TERMINAL_DETONATE") != NULL) {
                
                // CRITICAL FIRE POINT: Execute remote detonation under parachute canopy
                recovery_matrix.payload_vaporized = true;
                
                printf("\n=================================================================\n");
                printf("[CRITICAL REMOTE DETONATION VERIFIED] RF Command match found.\n");
                printf("-> SIGNAL ORIGIN: Remote Control Command Workstation.\n");
                printf("-> ACTION: Sending high-amperage current straight to core firing lines.\n");
                printf("-> STATUS: Weapon squibs detonated under descent canopy. Mission complete.\n");
                printf("=================================================================\n");
            }
        }
    } else {
        // If parachute is packed, keep remote radio channels safely locked to prevent stray signal triggers
        recovery_matrix.remote_detonation_bus_armed = false;
        recovery_matrix.environmental_safeties_bypassed = false;
    }
}
