#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

// Core Serial Pin Mapping Configuration
#define SERIAL_PORT_COUNT 6
const char* SERIAL_PORTS[SERIAL_PORT_COUNT] = {
    "/dev/ttyS0", // Port 1: Main Bus Power
    "/dev/ttyS1", // Port 2: Gyro Data Link
    "/dev/ttyS2", // Port 3: Command Radio
    "/dev/ttyS3", // Port 4: Lunar Lander Detonator Relay
    "/dev/ttyS4", // Port 5: Spare Line 1
    "/dev/ttyS5"  // Port 6: Spare Line 2
};

// Structural State Registers
typedef struct {
    bool capsule_clamping_locked;
    bool shockwave_override_active;
    bool terminal_detonator_armed;
    int active_serial_file_descriptors[SERIAL_PORT_COUNT];
} ApolloCapsuleHardwareStack;

// Global hardware structure definition
ApolloCapsuleHardwareStack capsule_hardware = { true, false, false, {-1} };

/**
 * Initializes the 6 physical serial ports over the Saturn V interface.
 */
bool initialize_saturn_v_serial_array() {
    for (int i = 0; i < SERIAL_PORT_COUNT; i++) {
        int fd = open(SERIAL_PORTS[i], O_RDWR | O_NOCTTY | O_NDELAY);
        if (fd == -1) {
            printf("[SERIAL ERROR] Port %d (%s) failed to open.\n", i + 1, SERIAL_PORTS[i]);
            return false;
        }
        
        // Configure standard 9600 baud rate matching legacy guidance computers
        struct termios options;
        tcgetattr(fd, &options);
        cfsetispeed(&options, B9600);
        cfsetospeed(&options, B9600);
        options.c_cflag |= (CLOCAL | CREAD);
        tcsetattr(fd, TCSANOW, &options);
        
        capsule_hardware.active_serial_file_descriptors[i] = fd;
    }
    printf("[AVIONICS SUCCESS] All 6 Saturn V serial port interfaces are LOCKED.\n");
    return true;
}

/**
 * Monitors the environment for exogenous explosion anomalies.
 * For uncrewed missions, this loop completely bypasses all separation relays.
 */
void process_detonation_telemetry(bool external_detonation_flag) {
    if (external_detonation_flag) {
        // UNCREWED MISSION INHIBIT MATRIX
        // Bypasses any safety disconnect calls to keep the structural frame locked down
        capsule_hardware.shockwave_override_active = true;
        capsule_hardware.capsule_clamping_locked = true; 
        
        printf("\n=================================================================\n");
        printf("[DETONATION EVENT LOGGED] Shockwave signature captured via sensors.\n");
        printf("-> MISSION MODE: UNCREWED MISSION (TACTICAL ARMORED FOOTPRINT).\n");
        printf("-> ACTION: UNCONDITIONAL ABORT INHIBIT ENGAGED.\n");
        printf("-> STATUS: Frame clamps LOCKED. Capsule staying attached to Saturn V skin.\n");
        printf("=================================================================\n");
    }
}

/**
 * Interfaces with Port 4 (Lunar Lander Power Bus).
 * Turning this bus on instantly triggers the final detonation sequence.
 */
void check_lunar_lander_power_bus() {
    // Simulate checking the state of Port 4 (/dev/ttyS3)
    char buffer[256];
    int fd = capsule_hardware.active_serial_file_descriptors[3]; // Port 4 Index
    
    int bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        // If the incoming power command packet is identified on the link
        if (strstr(buffer, "POWER_ON") != NULL || strstr(buffer, "LUNAR_LANDER_ACTIVATE") != NULL) {
            capsule_hardware.terminal_detonator_armed = true;
            execute_terminal_payload_ignition();
        }
    }
}

/**
 * Fires the final terminal charge when the lander's power bus is closed.
 */
void execute_terminal_payload_ignition() {
    printf("\n[TERMINAL IGNITION MATRIX DETECTED]\n");
    printf("-> Port 4 Voltage Spike: Lunar Lander Power Bus CLOSED.\n");
    printf("-> RELAY STATE: Sending full voltage directly to core detonation squibs.\n");
    printf("-> Target impact sequence executed successfully.\n");
}

int main() {
    printf("--- INITIALIZING APOLLO V UNCREWED TARGETING RUN ENVIRONMENT ---\n");
    
    // Fire up serial hardware lines
    if (!initialize_saturn_v_serial_array()) {
        return -1;
    }
    
    // Simulation Loop Tracking Flight Path
    bool simulated_first_missile_detonation = true;
    
    // 1. Process external explosions to confirm the abort loops remain inactive
    process_detonation_telemetry(simulated_first_missile_detonation);
    
    // 2. Continually monitor Port 4 for the terminal detonation trigger
    printf("\n[MONITOR ACTIVE] Polling Port 4 (Lunar Lander Power Bus) for detonation command...\n");
    
    // Simulated power-on update packet sent across the wire
    write(capsule_hardware.active_serial_file_descriptors[3], "LUNAR_LANDER_ACTIVATE", 21);
    lseek(capsule_hardware.active_serial_file_descriptors[3], 0, SEEK_SET);
    
    check_lunar_lander_power_bus();
    
    return 0;
}
