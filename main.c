/*
 * File:   main.c
 * Author: Amanda e Auro
 *
 * Descri��o: Este firmware escreve a string "AMANDA" em uma EEPROM externa 24LC256
 * via I2C, l� a mesma string de volta e a transmite via UART.
 *
 * - I2C: SCL1/SDA1 (Pinos 24/25 do PDIP28)
 * - UART TX: RP12 (Pino 23 do PDIP28)
 *
 * Created on 1 de Julho de 2025, 09:08
 */

#include "xc.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <libpic30.h> // Para a fun��o __delay_ms

//==============================================================================
// Configuration Bits
//==============================================================================
// Inicia com FRC, depois troca para FRC com PLL
_FOSCSEL(FNOSC_FRC); 
// Habilita a troca de clock, mas desabilita o monitoramento de falha. OSC2 � pino de clock. Prim�rio desabilitado.
_FOSC(FCKSM_CSECMD & OSCIOFNC_OFF & POSCMD_NONE); 
_FWDT(FWDTEN_OFF); // Watchdog Timer desabilitado
_FICD(JTAGEN_OFF & ICS_PGD1); // Desabilita JTAG, usa PGD1/PGC1 para debug

//==============================================================================
// Defini��es do Sistema e Perif�ricos
//==============================================================================
#define FCY 39625000UL // Frequ�ncia de ciclo de instru��o (Fosc/2)

// --- Defini��es I2C ---
#define I2C_BAUDRATE 100000UL // Baudrate do I2C (100kHz)
// I2CBRG = [(FCY/I2C_BAUDRATE) - (FCY/1,111,111)] - 1
#define I2C_BRG_VAL ((FCY/I2C_BAUDRATE) - (FCY/1111111UL)) - 1

// Endere�os da EEPROM 24LC256 (A2=A1=A0=0)
#define EEPROM_WRITE_ADDR 0b10100000 // Endere�o base + bit de escrita (0)
#define EEPROM_READ_ADDR  0b10100001 // Endere�o base + bit de leitura  (1)

// --- Defini��es UART ---
#define UART_BAUDRATE 9600
#define UART_BRG_VAL ((FCY/UART_BAUDRATE)/16)-1 // Modo de baixa velocidade (BRGH=0)
#define UART_BUFF_SIZE 128

//==============================================================================
// Estruturas e Vari�veis Globais
//==============================================================================
typedef struct {
    char buff[UART_BUFF_SIZE];
    volatile uint16_t length;
    volatile uint16_t index;
    volatile uint8_t busy;
} UARTHandler;

UARTHandler huart1;

// Defini��o dos dados e endere�o
uint16_t memory_address = 0x0000; // Endere�o de mem�ria para escrita/leitura
const uint8_t data_to_write[] = "AMANDA SOUZA E AURO ARAMIDES";
uint8_t data_length;

// Buffer para armazenar os dados lidos da EEPROM
char read_buffer[UART_BUFF_SIZE];
char message_buffer[UART_BUFF_SIZE];

//==============================================================================
// Prot�tipos de Fun��es
//==============================================================================
// Sistema
void OSC_Init(void);
void GPIO_Init(void);

// UART
void UART1_Init(void);
uint8_t UART1_SendString(char* str);

// I2C
void I2C1_Init(void);
void I2C1_Start(void);
void I2C1_Stop(void);
void I2C1_Restart(void);
void I2C1_Write(uint8_t data);
uint8_t I2C1_Read(void);
void I2C1_Ack(void);
void I2C1_Nack(void);
void I2C1_WaitForIdle(void);

// EEPROM
void EEPROM_WritePage(uint16_t addr, uint8_t *data, uint8_t length);
void EEPROM_ReadPage(uint16_t addr, uint8_t *buffer, uint8_t length);
void EEPROM_EraseAll(void);
void EEPROM_WaitWriteComplete(void) ;
//==============================================================================
// Fun��o Principal
//==============================================================================
int main(void) {
    // 1. Inicializa��o do sistema
    OSC_Init();
    GPIO_Init();
    I2C1_Init();
    UART1_Init();

//    EEPROM_EraseAll();  // Apaga tudo 
    data_length = strlen((char*)data_to_write);

    // Escreve na EEPROM
    EEPROM_WritePage(memory_address, data_to_write, data_length);
    EEPROM_WaitWriteComplete();  

    while(1) {
        // O microcontrolador pode entrar em modo de baixo consumo aqui.
    }
    return 0;
}

//==============================================================================
// Rotinas de Interrup��o (ISRs)
//==============================================================================
void __attribute__((__interrupt__, no_auto_psv)) _U1TXInterrupt(void) {
    IFS0bits.U1TXIF = 0;
    if (huart1.index < huart1.length) {
        U1TXREG = huart1.buff[huart1.index++];
    } else {
        huart1.busy = 0;
    }
}

void __attribute__((__interrupt__, no_auto_psv)) _INT0Interrupt(void) {
    IFS0bits.INT0IF = 0;  // Limpa flag
 
    // L� da EEPROM
    EEPROM_ReadPage(memory_address, (uint8_t*)read_buffer, data_length);
    read_buffer[data_length] = '\0'; // Adiciona o terminador nulo para formar uma string v�lida

    // Envia os dados lidos pela UART
    snprintf(message_buffer, UART_BUFF_SIZE, "Dado lido da EEPROM: %s\n\r", read_buffer);
    UART1_SendString(message_buffer);
}

//==============================================================================
// Fun��es de Inicializa��o
//==============================================================================
void OSC_Init(void) {
    PLLFBD = 41; // M = 43
    CLKDIVbits.PLLPOST = 0; // N1 = 2
    CLKDIVbits.PLLPRE = 0;  // N2 = 2
    __builtin_write_OSCCONH(0x01);
    __builtin_write_OSCCONL(OSCCON | 0x01);
    while (OSCCONbits.OSWEN);
    while (OSCCONbits.LOCK != 1);
}

void GPIO_Init(void) {
    AD1PCFGL = 0xFFF; // Pinos anal�gicos como digitais
    TRISBbits.TRISB7 = 1;      // RB7 como entrada (INT0)

    // Configura INT0 no RB7
    INTCON2bits.INT0EP = 1;    // Interrup��o na borda de descida
    IFS0bits.INT0IF = 0;       // Limpa flag
    IEC0bits.INT0IE = 1;       // Habilita INT0
}




void UART1_Init(void) {
    // Mapeia a fun��o de transmiss�o da UART1 (U1TX) para o pino RP12
    RPOR6bits.RP12R = 3; // 3 � o c�digo para U1TX
    TRISBbits.TRISB12 = 0; // Configura o pino RP12 como sa�da

    U1MODEbits.STSEL = 0;
    U1MODEbits.PDSEL = 0;
    U1MODEbits.ABAUD = 0;
    U1MODEbits.BRGH = 0;
    U1BRG = UART_BRG_VAL;
    U1STAbits.UTXISEL0 = 0;
    U1STAbits.UTXISEL1 = 0;
    IEC0bits.U1TXIE = 1;
    U1MODEbits.UARTEN = 1;
    U1STAbits.UTXEN = 1;
}

void I2C1_Init(void) {
    I2C1BRG = (uint16_t)I2C_BRG_VAL;
    I2C1CONbits.I2CEN = 1;
    I2C1CONbits.DISSLW = 1; // Desabilita slew rate para modo padr�o
}

//==============================================================================
// Fun��es de Comunica��o I2C
//==============================================================================
void I2C1_WaitForIdle(void) {
    while (I2C1CONbits.SEN || I2C1CONbits.PEN || I2C1CONbits.RCEN || 
           I2C1CONbits.RSEN || I2C1CONbits.ACKEN || I2C1STATbits.TRSTAT);
}

void I2C1_Start(void) {
    I2C1_WaitForIdle();
    I2C1CONbits.SEN = 1;
    while(I2C1CONbits.SEN);
}

void I2C1_Stop(void) {
    I2C1_WaitForIdle();
    I2C1CONbits.PEN = 1;
    while(I2C1CONbits.PEN);
}

void I2C1_Restart(void) {
    I2C1_WaitForIdle();
    I2C1CONbits.RSEN = 1;
    while(I2C1CONbits.RSEN);
}

void I2C1_Write(uint8_t data) {
    I2C1_WaitForIdle();
    I2C1TRN = data;
    while(I2C1STATbits.TBF); // Espera o buffer de transmiss�o ficar vazio
    I2C1_WaitForIdle();
    // Adicionado verifica��o de ACK
    if (I2C1STATbits.ACKSTAT) { // 1 = NACK recebido
       // Tratar erro de NACK se necess�rio
    }
}

uint8_t I2C1_Read(void) {
    I2C1_WaitForIdle();
    I2C1CONbits.RCEN = 1;
    while(!I2C1STATbits.RBF);
    return I2C1RCV;
}

void I2C1_Ack(void) {
    I2C1_WaitForIdle();
    I2C1CONbits.ACKDT = 0;
    I2C1CONbits.ACKEN = 1;
    while(I2C1CONbits.ACKEN);
}

void I2C1_Nack(void) {
    I2C1_WaitForIdle();
    I2C1CONbits.ACKDT = 1;
    I2C1CONbits.ACKEN = 1;
    while(I2C1CONbits.ACKEN);
}

//==============================================================================
// Fun��es de Acesso � EEPROM
//==============================================================================
void EEPROM_WritePage(uint16_t addr, uint8_t *data, uint8_t length) {
    if (length > 64) length = 64;
    I2C1_Start();
    I2C1_Write(EEPROM_WRITE_ADDR);
    I2C1_Write((addr >> 8) & 0xFF); // Endere�o Alto
    I2C1_Write(addr & 0xFF);        // Endere�o Baixo
    for (uint8_t i = 0; i < length; i++) {
        I2C1_Write(data[i]);
    }
    I2C1_Stop();
}

void EEPROM_ReadPage(uint16_t addr, uint8_t *buffer, uint8_t length) {
    // 1. "Dummy Write" para posicionar o ponteiro de endere�o da EEPROM
    I2C1_Start();
    I2C1_Write(EEPROM_WRITE_ADDR);
    I2C1_Write((addr >> 8) & 0xFF); // Endere�o Alto
    I2C1_Write(addr & 0xFF);        // Endere�o Baixo

    // 2. Reinicia a comunica��o para iniciar a leitura
    I2C1_Restart();
    I2C1_Write(EEPROM_READ_ADDR); // Envia endere�o do dispositivo com bit de leitura

    // 3. L� os bytes sequencialmente
    for (uint8_t i = 0; i < length; i++) {
        buffer[i] = I2C1_Read();
        if (i < (length - 1)) {
            I2C1_Ack(); // Envia ACK para continuar lendo
        } else {
            I2C1_Nack(); // Envia NACK no �ltimo byte para sinalizar o fim
        }
    }
    I2C1_Stop();
}


void EEPROM_EraseAll(void) 
{
    uint8_t blank[64];
    for (uint8_t i = 0; i < 64; i++) blank[i] = 0xFF;

    for (uint16_t addr = 0; addr < 0x8000; addr += 64) {
        EEPROM_WritePage(addr, blank, 64);

        // Aguarda EEPROM ficar pronta
        while (1) {
            I2C1_Start();
            I2C1_Write(EEPROM_WRITE_ADDR);
            I2C1_Stop();
            if (!I2C1STATbits.ACKSTAT) break;
        }
    }
}


void EEPROM_WaitWriteComplete(void) 
{
    do {
        I2C1_Start();
        I2C1_Write(EEPROM_WRITE_ADDR); // Tenta fazer um write dummy
        I2C1_Stop();
    } while (I2C1STATbits.ACKSTAT); // Enquanto n�o receber ACK, ainda est� ocupada
}


//==============================================================================
// Fun��es de Comunica��o UART
//==============================================================================
uint8_t UART1_SendString(char* str) {
    if (huart1.busy) return 0;
    uint16_t len = strlen(str);
    if (len == 0 || len >= UART_BUFF_SIZE) return 0;
    
    memcpy(huart1.buff, str, len);
    huart1.length = len;
    huart1.index = 1;
    huart1.busy = 1;
    
    U1TXREG = huart1.buff[0];
    return 1;
}
