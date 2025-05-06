// Proyecto: Sistema de cruce peatonal con semáforos y multiplexado de displays

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

// === Pines de botones ===
#define BOTON_1 11
#define BOTON_2 12

// === Semáforos peatonales ===
#define VERDE_P1 13
#define ROJO_P1  14
#define VERDE_P2 15
#define ROJO_P2  16

// === Semáforos vehiculares ===
#define VERDE_V1    17
#define AMARILLO_V1 18
#define ROJO_V1     19
#define VERDE_V2    20
#define AMARILLO_V2 21
#define ROJO_V2     22

// === Pines de segmentos del display ===
#define NUM_SEGMENTS 7
const uint SEGMENTS[NUM_SEGMENTS] = {2, 3, 4, 5, 6, 7, 8};

// === Control de displays ===
const uint DISPLAY_1_CTRL = 9;
const uint DISPLAY_2_CTRL = 10;

// Tabla de segmentos para números 0-9 (ánodo común)
const bool DIGITS[10][NUM_SEGMENTS] = {
    {1,1,1,1,1,1,0}, {0,1,1,0,0,0,0}, {1,1,0,1,1,0,1}, {1,1,1,1,0,0,1},
    {0,1,1,0,0,1,1}, {1,0,1,1,0,1,1}, {1,0,1,1,1,1,1}, {1,1,1,0,0,0,0},
    {1,1,1,1,1,1,1}, {1,1,1,1,0,1,1}
};

int counter = -1;
int active_display = 0;
int estado_semaforo[2] = {0, 2};
int tiempo_estado[2] = {0, 0};
int tiempo_cruce = 0;
bool parpadeo_verde[2] = {false, false};

void set_segments(int number) {
    for (int i = 0; i < NUM_SEGMENTS; i++) {
        gpio_put(SEGMENTS[i], DIGITS[number][i]);
    }
}

void clear_display() {
    for (int i = 0; i < NUM_SEGMENTS; i++) {
        gpio_put(SEGMENTS[i], 0);
    }
    gpio_put(DISPLAY_1_CTRL, 0);
    gpio_put(DISPLAY_2_CTRL, 0);
}

void init_gpio() {
    for (int i = 0; i < NUM_SEGMENTS; i++) {
        gpio_init(SEGMENTS[i]);
        gpio_set_dir(SEGMENTS[i], GPIO_OUT);
    }

    gpio_init(DISPLAY_1_CTRL);
    gpio_set_dir(DISPLAY_1_CTRL, GPIO_OUT);
    gpio_init(DISPLAY_2_CTRL);
    gpio_set_dir(DISPLAY_2_CTRL, GPIO_OUT);

    gpio_init(BOTON_1);
    gpio_set_dir(BOTON_1, GPIO_IN);
    gpio_pull_up(BOTON_1);

    gpio_init(BOTON_2);
    gpio_set_dir(BOTON_2, GPIO_IN);
    gpio_pull_up(BOTON_2);

    int leds[] = {VERDE_P1, ROJO_P1, VERDE_P2, ROJO_P2,
                  VERDE_V1, AMARILLO_V1, ROJO_V1,
                  VERDE_V2, AMARILLO_V2, ROJO_V2};
    for (int i = 0; i < 10; i++) {
        gpio_init(leds[i]);
        gpio_set_dir(leds[i], GPIO_OUT);
    }

    gpio_put(ROJO_P1, 1);
    gpio_put(ROJO_P2, 1);
    gpio_put(VERDE_V1, 1);
    gpio_put(ROJO_V2, 1);
}

void multiplex_display() {
    gpio_put(DISPLAY_1_CTRL, 0);
    gpio_put(DISPLAY_2_CTRL, 0);

    if (counter >= 0) {
        set_segments(counter);
        if (active_display == 1) {
            gpio_put(DISPLAY_1_CTRL, 1);
        } else if (active_display == 2) {
            gpio_put(DISPLAY_2_CTRL, 1);
        }
    }
}

void ciclo_semaforo(int cruce) {
    if (active_display == cruce) return;

    int verde = (cruce == 1) ? VERDE_V1 : VERDE_V2;
    int amarillo = (cruce == 1) ? AMARILLO_V1 : AMARILLO_V2;
    int rojo = (cruce == 1) ? ROJO_V1 : ROJO_V2;

    tiempo_estado[cruce - 1]++;

    switch (estado_semaforo[cruce - 1]) {
        case 0:
            if (tiempo_estado[cruce - 1] >= 650) {
                parpadeo_verde[cruce - 1] = true;
            }
            if (tiempo_estado[cruce - 1] >= 800) {
                gpio_put(verde, 0);
                gpio_put(amarillo, 1);
                estado_semaforo[cruce - 1] = 1;
                tiempo_estado[cruce - 1] = 0;
                parpadeo_verde[cruce - 1] = false;
            }
            if (parpadeo_verde[cruce - 1]) {
                gpio_put(verde, tiempo_estado[cruce - 1] % 20 < 10);
            }
            break;
        case 1:
            if (tiempo_estado[cruce - 1] >= 200) {
                gpio_put(amarillo, 0);
                gpio_put(rojo, 1);
                estado_semaforo[cruce - 1] = 2;
                tiempo_estado[cruce - 1] = 0;
            }
            break;
        case 2:
            if (tiempo_estado[cruce - 1] >= 800) {
                gpio_put(rojo, 0);
                gpio_put(verde, 1);
                estado_semaforo[cruce - 1] = 0;
                tiempo_estado[cruce - 1] = 0;
            }
            break;
    }
}

void forzar_rojo(int cruce) {
    int verde = (cruce == 1) ? VERDE_V1 : VERDE_V2;
    int amarillo = (cruce == 1) ? AMARILLO_V1 : AMARILLO_V2;
    int rojo = (cruce == 1) ? ROJO_V1 : ROJO_V2;

    gpio_put(verde, 0);
    gpio_put(amarillo, 0);
    gpio_put(rojo, 1);
    estado_semaforo[cruce - 1] = 2;
    tiempo_estado[cruce - 1] = 0;
}

void activar_cruce(int cruce) {
    int verde_p = (cruce == 1) ? VERDE_P1 : VERDE_P2;
    int rojo_p  = (cruce == 1) ? ROJO_P1  : ROJO_P2;

    forzar_rojo(cruce);
    gpio_put(rojo_p, 0);
    gpio_put(verde_p, 1);
    tiempo_cruce = 0;
}

void desactivar_cruce(int cruce) {
    int verde_v = (cruce == 1) ? VERDE_V1 : VERDE_V2;
    int rojo_v  = (cruce == 1) ? ROJO_V1  : ROJO_V2;
    int verde_p = (cruce == 1) ? VERDE_P1 : VERDE_P2;
    int rojo_p  = (cruce == 1) ? ROJO_P1  : ROJO_P2;

    gpio_put(verde_p, 0);
    gpio_put(rojo_p, 1);
    gpio_put(rojo_v, 0);
    gpio_put(verde_v, 1);
    estado_semaforo[cruce - 1] = 0;
    tiempo_estado[cruce - 1] = 0;
}

int main() {
    stdio_init_all();
    init_gpio();
    absolute_time_t last_time = get_absolute_time();

    while (true) {
        if (active_display == 0) {
            if (!gpio_get(BOTON_1)) {
                active_display = 1;
                counter = 9;
                activar_cruce(1);
            } else if (!gpio_get(BOTON_2)) {
                active_display = 2;
                counter = 9;
                activar_cruce(2);
            }
        }

        if (absolute_time_diff_us(last_time, get_absolute_time()) > 2000000) {
            if (counter > 0) {
                counter--;
            } else if (counter == 0) {
                desactivar_cruce(active_display);
                counter = -1;
                active_display = 0;
                clear_display();
            }
            last_time = get_absolute_time();
        }

        ciclo_semaforo(1);
        ciclo_semaforo(2);
        multiplex_display();
        sleep_ms(5);
    }
}
