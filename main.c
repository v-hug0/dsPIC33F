/*
 * File:   main.c
 * Author: Victor
 *
 * Created on 11 de Abril de 2025, 18:02
 */

#include "xc.h"
#include "p33FJ12MC202.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

//==============================================================================
// Configuration Bits Summary (with practical descriptions)
//==============================================================================

/*
 * _FOSCSEL(...) - Oscillator Startup Selection
 * --------------------------------------------------
 * FNOSC_FRC        -> Uses the internal Fast RC oscillator (approx. 7.37 MHz).
 *                     Good for basic operation without external components.
 * FNOSC_FRCPLL     -> Same as above, but with PLL for higher frequency.
 * FNOSC_PRI        -> Uses an external oscillator (XT, HS, or EC modes).
 *                     Required for precise clock sources.
 * FNOSC_PRIPLL     -> External oscillator + PLL. Best for high-speed, accurate clocks.
 * FNOSC_SOSC       -> Uses secondary low-power oscillator (usually 32.768 kHz).
 *                     Common in low-power or RTC applications.
 * FNOSC_LPRC       -> Low-power internal RC. Very low frequency, good for sleep modes.
 * FNOSC_FRCDIV16   -> Internal FRC divided by 16. Lowers frequency for power saving.
 * FNOSC_LPRCDIVN   -> Internal FRC divided by N (specific divider).
 * IESO_ON          -> Starts with FRC and switches automatically to selected oscillator.
 *                     Useful during startup when external oscillator takes time to stabilize.
 * IESO_OFF         -> Starts directly with the configured oscillator.
 */

/*
 * _FOSC(...) - Oscillator Configuration
 * --------------------------------------------------
 * POSCMD_EC        -> External clock input (driven by an external clock signal).
 *                     Used when the clock is provided by another device.
 * POSCMD_XT        -> Uses an external crystal/resonator in XT mode (mid-frequency).
 * POSCMD_HS        -> High-Speed mode for external crystal. For higher frequency crystals.
 * POSCMD_NONE      -> Disables the primary oscillator. Useful if not using external clock.

 * OSCIOFNC_ON      -> OSC2 pin works as a general-purpose digital I/O pin.
 * OSCIOFNC_OFF     -> OSC2 outputs the system clock. Useful for debugging clock output.

 * IOL1WAY_ON       -> Peripheral Pin Select (PPS) can only be configured once after reset.
 *                     Adds safety for pin assignment.
 * IOL1WAY_OFF      -> PPS can be reconfigured at runtime. More flexible but riskier.

 * FCKSM_CSECME     -> Enables both Clock Switching and Fail-Safe Clock Monitor.
 *                     Allows runtime clock source changes and detects oscillator failure.
 * FCKSM_CSECMD     -> Allows clock switching, but disables fail-safe monitoring.
 * FCKSM_CSDCMD     -> Disables both features. Clock is fixed and no failure detection.
 */

// Internal FRC at POR
_FOSCSEL(FNOSC_FRCPLL); 
// Enable Clock Switching and Configure Primary Oscillator in XT mode
_FOSC(FCKSM_CSECMD & OSCIOFNC_OFF & POSCMD_NONE);

#define FCY  40000000
#define BAUDRATE 9600
#define BRGVAL ((FCY/BAUDRATE)/16)-1 // Low speed mode

#define _UART_BUFF_SIZE 128
typedef struct{
    char buff[_UART_BUFF_SIZE];
    volatile uint16_t length;
    volatile uint16_t index;
    volatile uint8_t busy:2;
}UARTHandler;

UARTHandler huart1;
 unsigned int timePeriod = 0;
 char msg[64];


// Prototipo de funcoes
void PLL_Init(void);
void GPIO_Init(void);
void TIMER2_Init(void);
void INPUT_CAP_Init(void);
void UART_Init(void);
void UART_TX_Init(void);
uint8_t sendString(char* str, UARTHandler* handler);

unsigned int i;

int main(void) {
    PLL_Init();
    GPIO_Init();
    TIMER2_Init();
    INPUT_CAP_Init();
    UART_Init();
    while (1) {
        // burns clock cycle   
    }
    return 0;
}

 void __attribute__((__interrupt__, no_auto_psv)) _U1TXInterrupt(void)
 {
    IFS0bits.U1TXIF = 0; // clear TX interrupt flag
    if (huart1.index < huart1.length) {
        U1TXREG = huart1.buff[huart1.index++];
    } else {
        huart1.busy = 0;
    }
 }

// Essa interrup??o s? vai entrar a cada dois estouros (ou seja, com duas posi??es do buffer cheias)
void __attribute__((interrupt, no_auto_psv)) _IC1Interrupt(void)
{
	IFS0bits.IC1IF = 0;
	unsigned int t1,t2;
	t1 = IC1BUF;
	t2 = IC1BUF;
	if(t2>t1)
		timePeriod = t2-t1;
	else // ? FIFO (o ponteiro muda), e o contador n?o reseta
		timePeriod = (PR2-t1)+t2; 
    
    t2=t1;
    
    if (timePeriod != 0) {
        unsigned long freq_hz = FCY/(256*(unsigned long)timePeriod);
        unsigned long freq_rpm = freq_hz*60;
        sprintf(msg,"Velocidade = %lu RPM \n\r", freq_rpm);
        sendString(msg,&huart1);
    }
}

void PLL_Init(void)
{
    PLLFBD = 41;                    // M = 43
    CLKDIVbits.PLLPOST  = 0;        // N1 = 2
    CLKDIVbits.PLLPRE   = 0;        // N2 = 2
    while (OSCCONbits.COSC != 0b001);
};

void GPIO_Init(void)
{
    TRISB = 0;
    
    TRISBbits.TRISB13 = 1;       // RB1 configurado como Entrada
    RPINR7bits.IC1R = 0b1101;   // RB1/RP1 mapeado para Input Capture 1
    
    TRISBbits.TRISB9 = 0;       // RB9 configurado como Saída
    RPOR1bits.RP3R = 0b00011;   // TX UART
    
    //TRISBbits.TRISB10 = 1;      // RX entrada
    //RPINR18bits.U1RXR = 0b1010; // RX UART mapeada
};

void TIMER2_Init(void)
{
    T2CONbits.TON       = 0;
    T2CONbits.T32       = 0;
    T2CONbits.TCS       = 0;
    T2CONbits.TGATE     = 0;
    T2CONbits.TCKPS     = 0b11;
    PR2                 = 0xFFFF;
    TMR2                = 0;
    //IPC1bits.T2IP       = 0x01;
    IFS0bits.T2IF       = 0;
    IEC0bits.T2IE       = 0;
    T2CONbits.TON       = 1;
}

void INPUT_CAP_Init(void)
{    
    // Initialize the input capture module
    IC1CONbits.ICM = 0b00; // Disable Input Capture 1 module
    IC1CONbits.ICTMR = 1; // Select Timer2 as the IC1 Time Base
    IC1CONbits.ICI = 0b01; // Interrupt on every second capture event
    IC1CONbits.ICM = 0b011; // Generate capture event on every rising edge
    // Enable capture interrupt and timer 2
    IPC0bits.IC1IP = 1; // Setup IC1 interrupt priority level
    IFS0bits.IC1IF = 0; // Clear IC1 interrupt status flag
    IEC0bits.IC1IE = 1; // Enable IC1 interrupt
}

void UART_Init(void)
{
    U1MODEbits.STSEL    = 0;        // 1-stop bit
    U1MODEbits.PDSEL    = 0;        // No Parity, 8-data bits
    U1MODEbits.ABAUD    = 0;        // Auto-Baud Disabled
    U1MODEbits.BRGH     = 0;        // Low Speed mode
    U1BRG = BRGVAL;                 // BAUD Rate Setting for 9600
    UART_TX_Init();
    //UART_RX_Init();
    while(1);
}

void UART_TX_Init(void)
{
    U1STAbits.UTXISEL0  = 0;        // Interrupt after one Tx character is 
    // transmitted
    U1STAbits.UTXISEL1  = 0;
    IEC0bits.U1TXIE     = 1;        // Enable UART Tx interrupt
    U1MODEbits.UARTEN   = 1;        // Enable UART
    U1STAbits.UTXEN     = 1;        // Enable UART Tx
}


uint8_t sendString(char* str, UARTHandler* handler)
{
    // Return if UART is currently busy
    if (handler->busy) return 0;
    // Get string length
    uint16_t len = strlen(str);
    if (len == 0 || len >= _UART_BUFF_SIZE) return 0;
    // Safe copy to buffer, with boundary check
    for (uint16_t i = 0; i < len; i++) {
        handler->buff[i] = str[i];
    }
    handler->length = len;
    handler->index = 1;
    handler->busy = 1;
    
    // Start transmission by writing first character to UART
    U1TXREG = handler->buff[0];
    return 1;
}
