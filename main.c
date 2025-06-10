/*
 * Projeto 02 - Medição e Exibição de Tensão e Corrente com ADC (ADC+Serial)
 * Equipe: Victor Hugo (497553) e Eduardo Vilas Boas (509925)
 * 
 * Descrição: 
 * - Circuito composto por fonte de entrada de 5V  em série com um resistor de
 *   50 Ohms. Em série com este resistor é colocado um resistor shunt de 1 Ohm.
 * - Assim, a tensão sobre o shunt é lida em AN0, sendo a corrente.
 * - A tensão sobre a fonte é lida em AN1.
 * - Ambos valores são enviados pela UART.
 * 
 * 
 * 
 *
 * Configurações:
 * - Clock do sistema: 40MHz (FRCPLL com M=43, N1=2, N2=2)
 * - Resolução ADC: 10 bits (0-1023)
 */

#include "xc.h"
#include "p33FJ12MC202.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

//==============================================================================
// Configuration Bits - Configurações do oscilador
//==============================================================================

// Oscilador interno FRC com PLL (para 40 MHz)
_FOSCSEL(FNOSC_FRCPLL); 
// Clock switching habilitado, sem falha segura, OSC2 como clock, sem oscilador externo
_FOSC(FCKSM_CSECMD & OSCIOFNC_OFF & POSCMD_NONE);

//==============================================================================
// Constantes do sistema
//==============================================================================

#define ADC_RESOLUTION 1023     // 10 bits (0?1023)
#define VREF           5        // Tensão de referência do ADC (5V)
#define SHUNT_RES      1        // Resistor shunt de 1 Ohm

#define FCY 40000000            // Frequência do sistema (40 MHz)
#define BAUDRATE 9600           // Baud rate para comunicação UART
#define BRGVAL ((FCY/BAUDRATE)/16)-1 // Valor para registrador de baudrate

#define _UART_BUFF_SIZE 128     // Tamanho do buffer UART

//==============================================================================
// Estrutura para controle da UART
//==============================================================================

typedef struct{
    char buff[_UART_BUFF_SIZE];     // Buffer de envio
    volatile uint16_t length;       // Comprimento da string a ser enviada
    volatile uint16_t index;        // Índice de envio atual
    volatile uint8_t busy:2;        // Flag de ocupado
} UARTHandler;

UARTHandler huart1;

//==============================================================================
// Prototipação das funções
//==============================================================================

void PLL_Init(void);
void GPIO_Init(void);
void AD_Init(void);
uint16_t ADC_Read(uint8_t channel);
void UART_TX_Init(void);
void UART_Init(void);
uint8_t sendString(char* str, UARTHandler* handler);
float getCurrent(uint16_t current);
float getVoltage(uint16_t voltage);
void sendMeasurements(float current, float voltage);

//==============================================================================
// Variáveis Globais
//==============================================================================

uint16_t adcCurrent;
uint16_t adcVoltage;
unsigned int i;

//==============================================================================
// Função Principal
//==============================================================================

int main(void) {
    PLL_Init();       // Inicializa PLL para 40 MHz
    GPIO_Init();      // Configura os pinos
    AD_Init();        // Inicializa o ADC
    UART_Init();      // Inicializa a UART
    
    while (1) { 
        adcCurrent = ADC_Read(0);                    // Lê corrente em AN0
        adcVoltage = ADC_Read(1);                    // Lê tensão em AN1
        float current = getCurrent(adcCurrent);      // Converte ADC para corrente
        float voltage = getVoltage(adcVoltage);      // Converte ADC para tensão
        sendMeasurements(current, voltage);          // Envia via UART
    }
    return 0;
}

//==============================================================================
// Interrupção de Transmissão UART
//==============================================================================

void __attribute__((__interrupt__, no_auto_psv)) _U1TXInterrupt(void) {
    IFS0bits.U1TXIF = 0; // Limpa flag de interrupção
    if (huart1.index < huart1.length) {
        U1TXREG = huart1.buff[huart1.index++]; // Envia próximo caractere
    } else {
        huart1.busy = 0; // Finaliza transmissão
    }
}

//==============================================================================
// Inicializa PLL para gerar 40 MHz a partir do FRC
//==============================================================================

void PLL_Init(void) {
    PLLFBD = 41;                   // M = 43
    CLKDIVbits.PLLPOST = 0;       // N1 = 2
    CLKDIVbits.PLLPRE = 0;        // N2 = 2
    while (OSCCONbits.COSC != 0b001); // Aguarda PLL travar (FRCPLL ativo)
}

//==============================================================================
// Inicializa GPIOs: AN0 e AN1 como entrada analógica; RB3 como saída UART
//==============================================================================

void GPIO_Init(void) {
    AD1PCFGL = 0xFFFF;            // Todos os pinos como digitais inicialmente
    AD1PCFGLbits.PCFG0 = 0;       // AN0 analógico
    AD1PCFGLbits.PCFG1 = 0;       // AN1 analógico
    TRISAbits.TRISA0 = 1;         // RA0 como entrada (corrente)
    TRISAbits.TRISA1 = 1;         // RA1 como entrada (tensão)
    TRISBbits.TRISB3 = 0;         // RB3 como saída (TX)
}

//==============================================================================
// Inicializa módulo ADC
//==============================================================================

void AD_Init(void) {
    AD1CON1 = 0x0000;             // Conversão manual
    AD1CON1bits.FORM = 0b00;      // Resultado em formato inteiro
    AD1CSSL = 0;                  // Sem varredura
    AD1CON2 = 0;                  // Ref = AVdd/AVss
    AD1CON3 = 0x0002;             // Tad = 3 Tcy (tempo de aquisição/conversão)
}

//==============================================================================
// Lê um canal analógico específico do ADC
//==============================================================================

uint16_t ADC_Read(uint8_t channel) {
    AD1CON1bits.ADON = 1;              // Liga o ADC
    AD1CHS0bits.CH0SA = channel;       // Seleciona o canal (0 ou 1)
    AD1CON1bits.SAMP = 1;              // Inicia amostragem
    for(int i = 0; i < 100; i++);      // Pequeno delay para aquisição
    AD1CON1bits.SAMP = 0;              // Inicia conversão
    while (!AD1CON1bits.DONE);         // Aguarda fim da conversão
    return ADC1BUF0;                   // Retorna valor convertido
}

//==============================================================================
// Inicializa UART
//==============================================================================

void UART_Init(void) {
    U1MODEbits.STSEL = 0;              // 1 stop bit
    U1MODEbits.PDSEL = 0;              // 8 bits sem paridade
    U1MODEbits.ABAUD = 0;              // Auto-baud desativado
    U1MODEbits.BRGH = 0;               // Modo low-speed
    U1BRG = BRGVAL;                    // Define baudrate
    UART_TX_Init();                    // Inicializa transmissão
}

//==============================================================================
// Inicializa transmissão UART e mapeamento de pino
//==============================================================================

void UART_TX_Init(void) {
    U1STAbits.UTXISEL0 = 0;            // Interrupção após 1 caractere transmitido
    U1STAbits.UTXISEL1 = 0;
    IEC0bits.U1TXIE = 1;               // Habilita interrupção de TX
    U1MODEbits.UARTEN = 1;             // Habilita UART
    U1STAbits.UTXEN = 1;               // Habilita transmissão
    TRISBbits.TRISB3 = 0;              // RB3 como saída
    RPOR1 = 0x0300;                    // Mapeia RP3 (RB3) para função U1TX
    U1TXREG = 0;
}

//==============================================================================
// Envia uma string via UART usando interrupção
//==============================================================================

uint8_t sendString(char* str, UARTHandler* handler) {
    if (handler->busy) return 0;               // Ignora se já estiver enviando
    uint16_t len = strlen(str);                // Obtém comprimento da string
    if (len == 0 || len >= _UART_BUFF_SIZE) return 0;

    for (uint16_t i = 0; i < len; i++) {
        handler->buff[i] = str[i];             // Copia para buffer interno
    }

    handler->length = len;
    handler->index = 1;
    handler->busy = 1;
    U1TXREG = handler->buff[0];                // Inicia transmissão com 1º caractere
    return 1;
}

//==============================================================================
// Converte leitura do ADC (corrente) para valor real em A
//==============================================================================

float getCurrent(uint16_t current) {
    float voltageShunt = ((float)current * VREF) / ADC_RESOLUTION;
    return voltageShunt / SHUNT_RES;           // I = V / R
}

//==============================================================================
// Converte leitura do ADC (tensão) para valor real em V
//==============================================================================

float getVoltage(uint16_t voltage) {
    float measuredVoltage = ((float)voltage * VREF) / ADC_RESOLUTION;
    return measuredVoltage;                    // Pode ser ajustado com divisor resistivo
}

//==============================================================================
// Formata e envia as medições de corrente e tensão via UART
//==============================================================================

void sendMeasurements(float current, float voltage) {
    char buffer[128];
    int current_int = (int)(current * 100.0);  // Converte para centésimos de A
    int voltage_int = (int)(voltage * 100.0);  // Converte para centésimos de V

    snprintf(buffer, sizeof(buffer),
             "Corrente: %d.%02d A, Tensao: %d.%02d V\r\n",
             current_int / 100, current_int % 100,
             voltage_int / 100, voltage_int % 100);

    sendString(buffer, &huart1);               // Envia via UART
}
