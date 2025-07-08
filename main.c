/*

* Projeto 3 - Estufa Inteligente (ADC + PWM + Interrupcao externa)
* Equipe: Victor Hugo (497553) e Eduardo Vilas Boas (509925)
*
* Descricao:
* - Leitura de temperatura (LM35 no AN2) e luminosidade (LDR no AN3)
* - PWM controla ventilador (PWM1H) e iluminacao (PWM2H)
* - Botao de emergencia (INT0) para desativar todos os sistemas
*
* Funcionamento:
* - Ventilador: velocidade proporcional a leitura AD de temperatura (mais quente = mais rapido)
* - Iluminacao: intensidade inversamente proporcional a leitura AD de luminosidade (mais escuro = mais luz)
* - Modo emergencia: desativa ambos sistemas quando o botao e pressionado
*
* Configuracoes:
* - Clock do sistema: 40MHz (FRCPLL com M=43, N1=2, N2=2)
* - Resolucao ADC: 10 bits (0-1023)
* - Frequencia PWM: 4 kHz
* - Interrupcao INT0: borda de descida
*
*/

#include "xc.h"
#include "p33FJ12MC202.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

//==============================================================================
// Configuration Bits Summary
//==============================================================================

/*
 * _FOSCSEL(...) - Selecao do Oscilador
 * FNOSC_FRCPLL   -> Usa oscilador interno FRC com PLL para maior frequencia
 */

/*
 * _FOSC(...) - Configuracao do Oscilador
 * FCKSM_CSECMD   -> Permite mudanca de clock em runtime
 * OSCIOFNC_OFF   -> OSC2 saida do clock do sistema
 * POSCMD_NONE    -> Oscilador primario desabilitado
 */

// Configura FRC com PLL como oscilador
_FOSCSEL(FNOSC_PRI); 
// Habilita mudanca de clock e configura oscilador
_FOSC(POSCMD_XT);

// Definicoes de constantes
#define Fosc 10000000           // Frequencia do sistema (40 MHz)
#define BAUDRATE 9600
#define BRGVAL ((Fosc/2/BAUDRATE)/16)-1      // low speed mode

// Variaveis globais
int x = 0;
int controle = 0b10100000; // 1010 -> memoria 000 -> end chip 0 -> R/W
int endM = 0x00;
int endL = 0x00;

int length = 25;
char Byte[] = {"Victor Hugo"};
int i,z;

void __attribute__((interrupt, shadow, no_auto_psv)) _INT0Interrupt(void)
{
    IFS0bits.INT0IF = 0;
    U1TXREG = 0x00;
    
    I2C1CONbits.SEN = 1;
    while(I2C1CONbits.SEN);
    while(I2C1STATbits.TRSTAT);
    I2C1TRN = controle;
    while(I2C1STATbits.TRSTAT);
    I2C1TRN = endM;
    while(I2C1STATbits.TRSTAT);
    I2C1TRN = 0x00;
    while(I2C1STATbits.TRSTAT);
    I2C1CONbits.RSEN = 1;           // Generate Restart
    while(I2C1CONbits.RSEN);
    while(I2C1STATbits.TRSTAT);
    I2C1TRN = controle | 1;
    while(I2C1STATbits.TBF);
    for(i = 0; i < length; i++)
    {
        while(I2C1STATbits.TRSTAT);
        I2C1CONbits.RCEN = 1;       // Enable Master Receive
    Nop();
    while(!I2C1STATbits.RBF);
        U1TXREG = I2C1RCV;
        for(z=0; z<450; z++);
        if(i<(length-1))
        {
            while(I2C1STATbits.TRSTAT);
            I2C1CONbits.ACKDT = 0;  // Set for ACK
            I2C1CONbits.ACKEN = 1;
        } else {
        while(I2C1STATbits.TRSTAT);
        I2C1CONbits.ACKDT = 1;      // Set for NotACK
        I2C1CONbits.ACKEN = 1;
        while(I2C1CONbits.ACKEN);   // Wait for ACK to complete
        I2C1CONbits.ACKDT = 0;      // Set for NotACK
        }
    }

    while(I2C1STATbits.TRSTAT);
    I2C1CONbits.PEN = 1;            // Generate Stop Condition
    while(I2C1CONbits.PEN);         // Wait for stop
    LATBbits.LATB0 = !LATBbits.LATB0;
    z++;
}

// Prototipos de funcoes


// Funcao principal
int main(void) {
    TRISB = 0;
    TRISBbits.TRISB7 = 1;
    AD1PCFGL = 0xFFFF;
    
    // Habilitar interrupção externa
    IFS0bits.INT0IF = 0;
    IEC0bits.INT0IE = 1;
    
    // Config. I2C
    I2C1BRG = 0x004f;
    I2C1CON = 0x9200;
    
    // start uart
    U1MODE = 0;
    U1STA = 0;
    U1MODEbits.STSEL = 0;
    U1MODEbits.PDSEL = 0b00;
    U1MODEbits.BRGH = 0;
    U1MODEbits.UEN = 0b00;   // 
    U1BRG = BRGVAL;     // baudrate setting
    // Transmissão
    U1STAbits.UTXISEL1 = 0;
    U1STAbits.UTXISEL0 = 0;
    RPOR6bits.RP12R = 0b00011;
    IFS0bits.U1TXIF = 0;
    IEC0bits.U1TXIE = 0;
    // Recepção
    U1STAbits.URXISEL = 0b00;
    RPINR18bits.U1RXR = 13;
    // Habilita
    U1MODEbits.UARTEN = 1;
    U1STAbits.UTXEN = 1;
    
    U1TXREG = 13;
    
    I2C1CONbits.SEN = 1;
    while(I2C1CONbits.SEN);
    while(I2C1STATbits.TRSTAT);
    I2C1TRN = controle;
    while(I2C1STATbits.TRSTAT);
    I2C1TRN = endM;
    while(I2C1STATbits.TRSTAT);
    I2C1TRN = endL;
    for(i = 0; i < length; i++)
    {
        while(I2C1STATbits.TRSTAT);
        I2C1TRN = Byte[i];
    }
    while(I2C1STATbits.TRSTAT);
    I2C1CONbits.PEN = 1;            // Generate Stop Condition
    while(I2C1CONbits.PEN);         // Wait for stop
    
    while (1);
    return 0;
}