#ifndef PINOUT_NONE_H_
#define PINOUT_NONE_H_

// Las definiciones/asignaciones de pines, se realizan para permitir
// la carga del sketch y la simulación. 
// No resultan funcionales con el robot real
#define X_STEP_PIN         -1 // D1 // GPIO5
#define X_DIR_PIN          -1 // D2 // GPIO4
#define X_ENABLE_PIN       -1 // D8 // GPIO15 debe ser BAJO en boot
#define X_MIN_PIN          -1 // D9 // GPIO3 debe ser ALTO en boot
#define X_MAX_PIN          -1
 
#define Y_STEP_PIN         -1 // D5 // GPIO14
#define Y_DIR_PIN          -1 // D6 // GPIO12
#define Y_ENABLE_PIN       -1 // D8 // GPIO15 debe ser BAJO en boot
#define Y_MIN_PIN          -1 // D9 // GPIO3 debe ser ALTO en boot
#define Y_MAX_PIN          -1

#define Z_STEP_PIN         -1 // D7 // GPIO13
#define Z_DIR_PIN          -1 // D3 // GPIO0 (debe estar en ALTO durante BOOT)
#define Z_ENABLE_PIN       -1 // D8 // GPIO15 debe ser BAJO en boot
#define Z_MIN_PIN          -1 // D9 // GPIO3 debe ser ALTO en boot
#define Z_MAX_PIN          -1

#define E0_STEP_PIN        -1
#define E0_DIR_PIN         -1
#define E0_ENABLE_PIN      -1
#define E0_MIN_PIN         -1

#define E1_STEP_PIN        -1
#define E1_DIR_PIN         -1
#define E1_ENABLE_PIN      -1

#define BYJ_PIN_0          -1
#define BYJ_PIN_1          -1
#define BYJ_PIN_2          -1
#define BYJ_PIN_3          -1

#define SERVO_PIN          -1

#define PUMP_PIN           -1
#define LASER_PIN          -1
#define LED_PIN            -1

#define SDPOWER            -1
#define SDSS               -1

#define FAN_PIN            -1

#define PS_ON_PIN          -1
#define KILL_PIN           -1

#define TEMP_0_PIN         -1   // ANALOG NUMBERING
#define TEMP_1_PIN         -1   // ANALOG NUMBERING

#endif
