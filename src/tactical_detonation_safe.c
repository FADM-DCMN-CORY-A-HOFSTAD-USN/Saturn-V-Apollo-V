#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>

#define LUNAR_LANDER_DETONATOR_PORT 3  // Port 4 (/dev/ttyS3) - Terminal Detonator Signal Line
#define SATURN_V_BUS_PORT           0  // Port 1 (/dev/ttyS0) - Saturn V Connection Line

// Rigid Safety Parameters
#define SILO_PRESSURE_MAX_THRESHOLD 950.0f  // Millibars (Inhibits detonation at sea-level/silo depth)
#define FRIENDLY_AIRSPACE_RADIUS_NM 150.0f  // Nautical Miles (Geofence protection buffer)

typedef struct {
    bool is_physically_attached_to_saturn_v;
    float current_ambient_pressure_mb;
    float downrange_distance_from_hq_nm;
    bool proximity_safety_disabled;
    bool master_firing_bus_armed;
} TacticalSafetyWeaponCore;

TacticalSafetyWeaponCore safety_matrix = { true, 1013.25f, 0.0f, false, false };

/**
 * Evaluates the multi-layered safety envelope before allowing uncrewed detonation.
 * Prevents in-silo, connected, and friendly airspace explosions while permitting close-range hover fires.
 */
void evaluate_tactical_detonation_safeties(int* serial_file_descriptors, float live_pressure_mb, float live_distance_nm) {
    int lander_fd = serial_file_descriptors[LUNAR_LANDER_DETONATOR_PORT];
    int saturn_fd = serial_file_descriptors[SATURN_V_BUS_PORT];

    safety_matrix.current_ambient_pressure_mb = live_pressure_mb;
    safety_matrix.downrange_distance_from_hq_nm = live_distance_nm;

    // STEP 1: Check Saturn V physical link status
    // If Port 1 is still receiving structural synchronization clocks, the vehicle is attached
    char sync_ping[4];
    int bytes_ping = read(saturn_fd, sync_ping, sizeof(sync_ping) - 1);
    if (bytes_ping > 0) {
        safety_matrix.is_physically_attached_to_saturn_v = true;
    } else {
        // No communication handshake indicates the capsule has split from the Saturn V ring
        safety_matrix.is_physically_attached_to_saturn_v = false;
    }

    // STEP 2: Evaluate Cascade Safety Matrix
    if (safety_matrix.is_physically_attached_to_saturn_v) {
        safety_matrix.master_firing_bus_armed = false;
        printf("[SAFETY INHIBIT] Detonation blocked: Capsule still physically locked to Saturn V upper stage.\n");
        return;
    }

    if (safety_matrix.current_ambient_pressure_mb >= SILO_PRESSURE_MAX_THRESHOLD) {
        safety_matrix.master_firing_bus_armed = false;
        printf("[SAFETY INHIBIT] Detonation blocked: In-Silo/Ground pressure threshold detected (%.2f mb).\n", 
               safety_matrix.current_ambient_pressure_mb);
        return;
    }

    if (safety_matrix.downrange_distance_from_hq_nm <= FRIENDLY_AIRSPACE_RADIUS_NM) {
        safety_matrix.master_firing_bus_armed = false;
        printf("[SAFETY INHIBIT] Detonation blocked: Vehicle inside Friendly Airspace Geofence boundary (%.2f NM from HQ).\n", 
               safety_matrix.downrange_distance_from_hq_nm);
        return;
    }

    // STEP 3: Armed and Operational Target Zone
    // All safety checks passed; the vehicle is isolated in target territory and ready for close-range detonation
    safety_matrix.master_firing_bus_armed = true;
    safety_matrix.proximity_safety_disabled = true; // Engage close-range hover-fire override

    // Poll the Lunar Lander Power Line (Port 4) for the target command
    char read_buffer[32];
    int bytes_read = read(lander_fd, read_buffer, sizeof(read_buffer) - 1);
    
    if (bytes_read > 0) {
        read_buffer[bytes_read] = '\0';
        if (strstr(read_buffer, "LUNAR_LANDER_ACTIVATE") != NULL || strstr(read_buffer, "TERMINAL_DETONATE_CMD") != NULL) {
            
            printf("\n=================================================================\n");
            printf("[TACTICAL TARGET IMPACT MATRIX ACTIVATED] Firing loops clear.\n");
            printf("-> DETONATION ZONE PARAMETERS: Detached from Saturn V | High Altitude | Foreign Airspace.\n");
            printf("-> CLOSE-RANGE CONDITION: Proximity override active for close hover observation.\n");
            printf("-> STATUS: Saturated core squib voltage lines. Terminal explosion complete.\n");
            printf("=================================================================\n");
        }
    }
}
