#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "hardware/pwm.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include "hardware/clocks.h"
#include <hardware/pio.h>
#include "animacao_matriz.pio.h" // Biblioteca PIO para controle de LEDs WS2818B 

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define ADC_JOYSTICK_X 26
#define ADC_JOYSTICK_Y 27
#define LED_RED 13
#define LED_BLUE 12
#define LED_GREEN 11
#define tam_quad 10

#define MATRIZ_PIN 7            // Pino GPIO conectado aos LEDs WS2818B
#define LED_COUNT 25            // Número de LEDs na matriz
#define max_value_joy 4065.0 // (4081 - 16) que são os valores extremos máximos lidos pelo meu joystick
#define BUZZER_A 21


// Declaração de variáveis globais
PIO pio;
uint sm;

// Matriz para armazenar os desenhos da matriz de LEDs
uint padrao_led[4][LED_COUNT] = {
    {0, 0, 1, 0, 0,
     0, 1, 2, 1, 0,
     1, 2, 2, 2, 1,
     0, 1, 2, 1, 0,
     0, 0, 1, 0, 0,
    }, // Perigo
    {0, 0, 0, 0, 0,
     0, 0, 0, 0, 0,
     0, 0, 0, 0, 0,
     0, 0, 0, 0, 0,
     0, 0, 0, 0, 0,
    }, // Desligado
};

// Ordem da matriz de LEDS, útil para poder visualizar na matriz do código e escrever na ordem correta do hardware
int ordem[LED_COUNT] = {0, 1, 2, 3, 4, 9, 8, 7, 6, 5, 10, 11, 12, 13, 14, 19, 18, 17, 16, 15, 20, 21, 22, 23, 24};  


// Rotina para definição da intensidade de cores do led
uint32_t matrix_rgb(unsigned r, unsigned g, unsigned b){
    return (g << 24) | (r << 16) | (b << 8);
}

// Rotina para desenhar o padrão de LED
void display_desenho(uint8_t desenho){
    uint32_t valor_led;
    for (int i = 0; i < LED_COUNT; i++){
        // Define a cor do LED de acordo com o padrão
        if (padrao_led[desenho][ordem[24 - i]] == 1){
            valor_led = matrix_rgb(20, 0, 0); // Vermelho
        } else if (padrao_led[desenho][ordem[24 - i]] == 2){
            valor_led = matrix_rgb(20, 20, 0); // Amarelo
        } else{
            valor_led = matrix_rgb(0, 0, 0); // Desliga o LED
        }
        // Atualiza o LED
        pio_sm_put_blocking(pio, sm, valor_led);
    }
}



typedef struct
{
    uint16_t volume;
    uint16_t nivel;
} data;

QueueHandle_t xQueueJoystickConvert;
QueueHandle_t bQueueLedAlerta;
QueueHandle_t bQueueBuzzerAlerta;
QueueHandle_t bQueueMatrizAlerta;
QueueHandle_t bQueueDisplayAlerta;

void vJoystickTask(void *params)
{
    adc_gpio_init(ADC_JOYSTICK_Y);
    adc_gpio_init(ADC_JOYSTICK_X);
    adc_init();

    data joydata;
    bool alerta;

    while (true)
    {
        adc_select_input(0); // GPIO 26 = ADC0
        joydata.nivel = adc_read();
        joydata.nivel = ((joydata.nivel - 16) / max_value_joy) * 100; // Converte o valor do eixo y para a faixa de 0 a 100

        adc_select_input(1); // GPIO 27 = ADC1
        joydata.volume = adc_read();
        joydata.volume = ((joydata.volume - 16) / max_value_joy) * 100; // Converte o valor do eixo x para a faixa de 0 a 100
        
        xQueueSend(xQueueJoystickConvert, &joydata, 0); // Envia o valor do joystick para a fila

        // Verifica se os limites estão acima
        if (joydata.nivel > 70 || joydata.volume > 80){
            alerta = true;
        } else{
            alerta = false;
        }

        xQueueSend(bQueueDisplayAlerta, &alerta, 0);
        xQueueSend(bQueueLedAlerta, &alerta, 0);
        xQueueSend(bQueueBuzzerAlerta, &alerta, 0);
        xQueueSend(bQueueMatrizAlerta, &alerta, 0);

        vTaskDelay(pdMS_TO_TICKS(100));              // 10 Hz de leitura
    }
}

void vDisplayTask(void *params)
{
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_t ssd;
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);

    data joydata;
    bool alerta = false;
    bool cor = true;
    while (true)
    {
        if (xQueueReceive(xQueueJoystickConvert, &joydata, portMAX_DELAY) == pdTRUE)
        {
            // Mensagem para mostrar no display
            char vol[20];
            char nivel[20];
            char modo[20];
            if(xQueueReceive(bQueueDisplayAlerta, &alerta, portMAX_DELAY) == pdTRUE){
                sprintf(vol, "V. chuva: %d%%", joydata.volume);
                sprintf(nivel, "N.  agua: %d%%", joydata.nivel);
                sprintf(modo, "Modo: %s", alerta ? "ALERTA!!" : "Normal");
                if (alerta){
                    cor = !cor;

                    ssd1306_fill(&ssd, cor);                        // Limpa a tela
                    ssd1306_rect(&ssd, 3, 3, 122, 58, !cor, cor); // Desenha um retângulo
                    ssd1306_draw_string(&ssd, vol, 8, 10); // Desenha uma string
                    ssd1306_draw_string(&ssd, nivel, 8, 20); // Desenha uma string
                    ssd1306_line(&ssd, 0, 32, 127, 32, true); // Desenha uma linha divisória no meio da tela
                    ssd1306_draw_string(&ssd, modo, 8, 40); // Desenha uma string
                    ssd1306_send_data(&ssd);
                } else{
                    ssd1306_fill(&ssd, true);                        // Limpa a tela
                    ssd1306_rect(&ssd, 3, 3, 122, 58, false, true); // Desenha um retângulo
                    ssd1306_draw_string(&ssd, vol, 8, 10); // Desenha uma string
                    ssd1306_draw_string(&ssd, nivel, 8, 20); // Desenha uma string
                    ssd1306_line(&ssd, 0, 32, 127, 32, true); // Desenha uma linha divisória no meio da tela
                    ssd1306_draw_string(&ssd, modo, 8, 40); // Desenha uma string
                    ssd1306_send_data(&ssd);
                }
            } 
        }
    }
}


void vLedTask(void *params)
{
    gpio_init(LED_RED);
    gpio_set_dir(LED_RED, GPIO_OUT);

    bool alerta;
    while (true){
        if (xQueueReceive(bQueueLedAlerta, &alerta, portMAX_DELAY) == pdTRUE){
            if (alerta){
                gpio_put(LED_RED, 1);
            } else{
                gpio_put(LED_RED, 0);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void vMatrizTask(void *params){
    pio = pio0; 
    uint offset = pio_add_program(pio, &animacao_matriz_program);
    sm = pio_claim_unused_sm(pio, true);
    animacao_matriz_program_init(pio, sm, offset, MATRIZ_PIN);

    bool alerta;

    while (true){
        if (xQueueReceive(bQueueMatrizAlerta, &alerta, portMAX_DELAY) == pdTRUE){
            if (alerta){
                // Liga com o padrão 0
                display_desenho(0);
            } else{
                // Desliga
                display_desenho(1);
            }
        }

        // Lógica para atualizar a matriz de LEDs
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void vBuzzerTask(void *params){
    // Configuração do buzzer
    gpio_set_function(BUZZER_A, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_A); // Obtém o slice correspondente
    pwm_set_clkdiv(slice_num, 125); // Define o divisor de clock
    pwm_set_wrap(slice_num, 1000);  // Define o valor máximo do PWM
    pwm_set_enabled(slice_num, true);
    
    bool alerta;
    bool som = false; // Para definir qual o som a ser tocado
    while (true){
        if (xQueueReceive(bQueueBuzzerAlerta, &alerta, portMAX_DELAY) == pdTRUE){
            if (alerta){
                if (som){
                    pwm_set_clkdiv(slice_num, 125); // Define o divisor de clock
                    pwm_set_wrap(slice_num, 1000);  // Define o valor máximo do PWM
                    pwm_set_gpio_level(BUZZER_A, 100);
                    vTaskDelay(pdMS_TO_TICKS(200));
                    som = !som;
                } else{
                    pwm_set_clkdiv(slice_num, 250); // Define o divisor de clock
                    pwm_set_wrap(slice_num, 1000);  // Define o valor máximo do PWM
                    pwm_set_gpio_level(BUZZER_A, 100);
                    vTaskDelay(pdMS_TO_TICKS(200));
                    som = !som;
                }
            }
            pwm_set_gpio_level(BUZZER_A, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}


int main()
{
    stdio_init_all();

    // Cria a fila para compartilhamento de valores
    xQueueJoystickConvert = xQueueCreate(5, sizeof(data));
    bQueueDisplayAlerta = xQueueCreate(5, sizeof(bool));
    bQueueLedAlerta = xQueueCreate(5, sizeof(bool));
    bQueueBuzzerAlerta = xQueueCreate(3, sizeof(bool));
    bQueueMatrizAlerta = xQueueCreate(5, sizeof(bool));

    // Criação das tasks
    xTaskCreate(vJoystickTask, "Joystick Task", 256, NULL, 1, NULL);
    xTaskCreate(vDisplayTask, "Display Task", 512, NULL, 1, NULL);
    xTaskCreate(vLedTask, "LED Task", 256, NULL, 1, NULL);
    xTaskCreate(vMatrizTask, "Matriz Task", 256, NULL, 1, NULL);
    xTaskCreate(vBuzzerTask, "Buzzer Task", 256, NULL, 1, NULL);

    // Inicia o agendador
    vTaskStartScheduler();
    panic_unsupported();
}
