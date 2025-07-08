/*
 * Projeto I2C - Montar uma rede de um DSPIC e uma memória I2C, gravar na memória o seu nome e depois ler ele e colocar em uma "STRING"
 * Equipe: Victor Hugo (497553) e Eduardo Vilas Boas (509925)
 * Descricao: É escrito uma string na memória por meio do protocolo I2C e um botão dispara
 * uma interrupção externa (INT0) para ler a memória I2C, transmitindo esse dado como string na UART.
*/


#include "xc.h"
#include "p33FJ12MC202.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

//==============================================================================
// Configura��es e defini��es
//==============================================================================
_FOSCSEL(FNOSC_PRI); 
_FOSC(POSCMD_XT);

#define Fosc 10000000
#define BAUDRATE 9600
#define BRGVAL ((Fosc/2/BAUDRATE)/16)-1

//==============================================================================
// Vari�veis globais
//==============================================================================
int controle = 0b10100000;  // Endere�o da EEPROM + Write
int endM = 0x00;
int endL = 0x00;

char Byte[] = {"Victor Hugo"};
volatile int leitura_pendente = 0;
int i;

//==============================================================================
// Prototipa��o
//==============================================================================
void I2C_Start(void);
void I2C_Stop(void);
void I2C_Restart(void);
void I2C_WriteByte(uint8_t byte);
uint8_t I2C_ReadByte(int ack);
void EEPROM_WriteString(const char* str);
void EEPROM_ReadString(char* buffer, int len);

//==============================================================================
// Interrup��o INT0
//==============================================================================
void __attribute__((interrupt, shadow, no_auto_psv)) _INT0Interrupt(void)
{
    IFS0bits.INT0IF = 0;
    leitura_pendente = 1;
}

//==============================================================================
// Fun��es I2C gen�ricas
//==============================================================================
void I2C_Start(void)
{
    I2C1CONbits.SEN = 1;
    while (I2C1CONbits.SEN);
}

void I2C_Stop(void)
{
    I2C1CONbits.PEN = 1;
    while (I2C1CONbits.PEN);
}

void I2C_Restart(void)
{
    I2C1CONbits.RSEN = 1;
    while (I2C1CONbits.RSEN);
}

void I2C_WriteByte(uint8_t byte)
{
    I2C1TRN = byte;
    while (I2C1STATbits.TRSTAT);
}

uint8_t I2C_ReadByte(int ack)
{
    I2C1CONbits.RCEN = 1;               // Habilita recep��o
    while (!I2C1STATbits.RBF);          // Espera byte dispon�vel

    uint8_t received = I2C1RCV;

    I2C1CONbits.ACKDT = !ack;           // ack=1 => ACK (ACKDT=0), ack=0 => NACK
    I2C1CONbits.ACKEN = 1;
    while (I2C1CONbits.ACKEN);

    return received;
}

//==============================================================================
// Fun��es EEPROM
//==============================================================================
void EEPROM_WriteString(const char* str)
{
    I2C_Start();
    I2C_WriteByte(controle);   // Endere�o EEPROM + Write
    I2C_WriteByte(endM);
    I2C_WriteByte(endL);
    for (int j = 0; j < strlen(str); j++)
    {
        I2C_WriteByte(str[j]);
    }
    I2C_Stop();
}

void EEPROM_ReadString(char* buffer, int len)
{
    I2C_Start();
    I2C_WriteByte(controle);     // Endere�o EEPROM + Write
    I2C_WriteByte(endM);
    I2C_WriteByte(endL);
    I2C_Restart();
    I2C_WriteByte(controle | 0x01);  // Endere�o EEPROM + Read

    for (int j = 0; j < len; j++)
    {
        buffer[j] = I2C_ReadByte(j < (len - 1)); // ACK exceto �ltimo byte
    }

    I2C_Stop();
}

//==============================================================================
// Fun��o principal
//==============================================================================
int main(void)
{
    char buffer[25];

    // Configura��o de pinos
    TRISB = 0;
    TRISBbits.TRISB7 = 1;      // Bot�o
    TRISBbits.TRISB0 = 0;      // LED
    LATBbits.LATB0 = 0;
    AD1PCFGL = 0xFFFF;

    // INT0
    IFS0bits.INT0IF = 0;
    IEC0bits.INT0IE = 1;

    // I2C
    I2C1BRG = 0x004F;
    I2C1CON = 0x9200;

    // UART
    U1MODE = 0;
    U1STA = 0;
    U1MODEbits.STSEL = 0;
    U1MODEbits.PDSEL = 0b00;
    U1MODEbits.BRGH = 0;
    U1MODEbits.UEN = 0b00;
    U1BRG = BRGVAL;
    RPOR6bits.RP12R = 0b00011;      // TX = RP12
    RPINR18bits.U1RXR = 13;         // RX = RP13
    U1MODEbits.UARTEN = 1;
    U1STAbits.UTXEN = 1;

    // Escrita inicial na EEPROM
    EEPROM_WriteString(Byte);

    // Loop principal
    while (1)
    {
        if (leitura_pendente)
        {
            leitura_pendente = 0;

            EEPROM_ReadString(buffer, strlen(Byte));

            // UART: envia string lida
            for (i = 0; i < strlen(Byte); i++)
            {
                while (U1STAbits.UTXBF);
                U1TXREG = buffer[i];
            }

            // Quebra de linha
            while (U1STAbits.UTXBF); U1TXREG = '\r';
            while (U1STAbits.UTXBF); U1TXREG = '\n';

            // Pisca LED
            LATBbits.LATB0 = !LATBbits.LATB0;
        }
    }
    return 0;
}
