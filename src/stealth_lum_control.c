#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>

#define LAUNCH_SEQUENCE_PORT  0  // Port 1 (/dev/ttyS0) - Reads master countdown tracking states
#define LUMINESCENT_RELAY_PORT 5  // Port 6 (/dev/ttyS5) - Controls complete lighting array power lines

// Core Tactical Vehicle Flight Phases
typedef enum {
    FLIGHT_PHASE_PRELAUNCH = 0,
    FLIGHT_PHASE_IGNITION_ACTIVE,
    FLIGHT_PHASE_STEALTH_TRANSIT,
    FLIGHT_PHASE_TOUCHDOWN_VERIFIED
} VehicleFlightPhase;

typedef struct {
    VehicleFlightPhase current_phase;
    bool all_vehicle_lights_enabled;
    float current_altitude_feet;
} TacticalStealthHardwareStack;

TacticalStealthHardwareStack stealth_core = { FLIGHT_PHASE_PRELAUNCH, true, 0.0f };

/**
 * High-priority luminescent blackout controller loop.
 * Cuts power to all lamps immediately before takeoff and suppresses illumination during transit.
 */
void process_stealth_illumination_matrix(int* serial_file_descriptors, float radar_altimeter_ft) {
    int launch_fd = serial_file_descriptors[LAUNCH_SEQUENCE_PORT];
    int lum_fd = serial_file_descriptors[LUMINESCENT_RELAY_PORT];
    
    stealth_core.current_altitude_feet = radar_altimeter_ft;

    // State Machine Loop evaluating countdown blocks and landing gates
    switch (stealth_core.current_phase) {
        
        case FLIGHT_PHASE_PRELAUNCH:
            // Monitor Port 1 for the T-0 minus terminal ignition sequence command packet
            char countdown_buffer[32];
            int bytes_read = read(launch_fd, countdown_buffer, sizeof(countdown_buffer) - 1);
            if (bytes_read > 0) {
                countdown_buffer[bytes_read] = '\0';
                if (strstr(countdown_buffer, "T_MINUS_ZERO_IGNITION") != NULL) {
                    // Transition to active escape/takeoff phase
                    stealth_core.current_phase = FLIGHT_PHASE_IGNITION_ACTIVE;
                }
            }
            break;

        case FLIGHT_PHASE_IGNITION_ACTIVE:
            // CRITICAL STEALTH INTERLOCK: Force total vehicle blackout milliseconds before launch
            stealth_core.all_vehicle_lights_enabled = false;
            
            // Assert a hard 0.0V low-side drop to pop open the primary lighting circuit relays
            write(lum_fd, "LIGHTING_RELAY_BUS_DISCONNECT_KILL\n", 35);
            
            printf("\n=================================================================\n");
            printf("[TACTICAL STEALTH INTERLOCK ENGAGED] Ignition sequence confirmed.\n");
            printf("-> THREAT RISK CONFIGURATION: ACTIVE VISUAL SUPPRESSION MODE.\n");
            printf("-> ACTION: Forcing all external navigation lamps and displays COLD.\n");
            printf("-> STATUS: Port 6 relay grounded to 0.0V. Full vehicle blackout active.\n");
            printf("=================================================================\n");
            
            stealth_core.current_phase = FLIGHT_PHASE_STEALTH_TRANSIT;
            break;

        case FLIGHT_PHASE_STEALTH_TRANSIT:
            // Suppress all illumination loops continuously while high-altitude altitude tracking is live
            if (stealth_core.current_altitude_feet <= 5.0f) {
                // If the altimeter returns down to zero, the vehicle has touched down at the recovery site
                stealth_core.current_phase = FLIGHT_PHASE_TOUCHDOWN_VERIFIED;
            }
            break;

        case FLIGHT_PHASE_TOUCHDOWN_VERIFIED:
            // Recovery confirmed; restore voltage to primary lighting grids safely
            stealth_core.all_vehicle_lights_enabled = true;
            write(lum_fd, "LIGHTING_RELAY_BUS_RECONNECT_RESTORE\n", 37);
            
            printf("\n[TACTICAL INFO] Recovery touchdown verified at landing coordinates.\n");
            printf("-> Action: Re-energizing primary electrical luminescent paths.\n");
            printf("-> Status: Navigation indicators active for hangar recovery teams.\n");
            
            stealth_core.current_phase = FLIGHT_PHASE_PRELAUNCH; // Cycle back to standby loops
            break;
    }
}
