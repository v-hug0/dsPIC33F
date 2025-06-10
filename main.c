/*
 * Projeto 01 - Controle de Velocidade de Motor DC
 * 
 * Descri��o:
 * - Utiliza um potenci�metro conectado em AN0 (RB0) para ajustar o duty cycle do PWM
 * - PWM controla a velocidade de um motor DC
 * - Inclui um bot�o em RB4 para simular a fun��o de freio
 * - Quando solto, retorna ao controle pelo potenci�metro
 * 
 * Configura��es:
 * - Clock do sistema: 40MHz (FRCPLL com M=43, N1=2, N2=2)
 * - Frequ�ncia PWM: 4kHz
 * - Resolu��o ADC: 10 bits (0-1023)
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

#define ADC_RESOLUTION 1023
#define VREF           5
#define SHUNT_RES      0.1

uint16_t adcCurrent; // Vari�vel para armazenar o valor lido do ADC
uint16_t adcVoltage;

#define FCY 40000000
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


// Prot�tipos de fun��es
void PLL_Init(void);      // Configura o PLL para gerar 40MHz
void GPIO_Init(void);     // Configura os pinos de I/O
void AD_Init(void);       // Configura o m�dulo ADC
uint16_t ADC_Read(uint8_t channel);      // Inicia convers�o ADC e retorna valor
void UART_TX_Init(void);
uint8_t sendString(char* str, UARTHandler* handler);
float getCurrent(uint16_t current);
float getVoltage(uint16_t voltage);
void sendMeasurements(float current, float voltage);
unsigned int i;

int main(void) {
    PLL_Init();
    GPIO_Init();
    AD_Init();   
    UART_Init();
    
    while (1) { 
        sendString("teste\n",&huart1);
        //adcCurrent = ADC_Read(0);
        //adcVoltage = ADC_Read(1);
        //float current = getCurrent(adcCurrent);
        //float voltage = getVoltage(adcVoltage);
        //sendMeasurements(150, 150);
        
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

void PLL_Init(void)
{
    PLLFBD = 41;                    // M = 43
    CLKDIVbits.PLLPOST  = 0;        // N1 = 2
    CLKDIVbits.PLLPRE   = 0;        // N2 = 2
    while (OSCCONbits.COSC != 0b001);
};

void GPIO_Init(void)
{
    TRISBbits.TRISB3 = 0;
    // Habilita o anal�gico apenas em AN0 e AN1
    AD1PCFGL = 0xFFFF;
    AD1PCFGLbits.PCFG0 = 0;
    AD1PCFGLbits.PCFG1 = 0;
    // Modo de saida do LED 
    TRISAbits.TRISA0 = 1;
    TRISAbits.TRISA1 = 1;
};

void AD_Init(void)
{
    AD1CON1 = 0x0000;   // SAMP bit = 0 ends sampling
                        // and starts converting
    AD1CSSL = 0;        
    AD1CON2 = 0;        // Voltage reference to Avdd e Avss
    AD1CON3 = 0x0002;   // Manual sample, Tad = internal 3 Tcy
    
}

uint16_t ADC_Read(uint8_t channel)
{
    int adc = 0;
    AD1CON1bits.ADON = 1; // Turn ADC ON
    AD1CHS0bits.CH0SA = channel;
    AD1CON1bits.SAMP = 1; // starts sampling
    for(int i = 0; i < 100; i++);
    AD1CON1bits.SAMP = 0; // start converting
    while (!AD1CON1bits.DONE);
    adc = ADC1BUF0;
    return adc;
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
    //while(1);
    
}

void UART_TX_Init(void)
{
    U1STAbits.UTXISEL0  = 0;        // Interrupt after one Tx character is 
    // transmitted
    U1STAbits.UTXISEL1  = 0;
    IEC0bits.U1TXIE     = 1;        // Enable UART Tx interrupt
    U1MODEbits.UARTEN   = 1;        // Enable UART
    U1STAbits.UTXEN     = 1;        // Enable UART Tx
    TRISBbits.TRISB3    = 0;        // TX in RP3
    RPOR1 = 0x0300;
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

float getCurrent(uint16_t current) {
    // Converter valor ADC para tensão no shunt
    float voltageShunt = (current * VREF) / ADC_RESOLUTION;
    // Calcular corrente usando Lei de Ohm (I = V/R)
    // Considerando amplificação se houver (não mencionado no circuito)
    return voltageShunt / SHUNT_RES;
}

float getVoltage(uint16_t voltage) {
    // Converter valor ADC para tensão medida
    float measuredVoltage = (voltage * VREF) / ADC_RESOLUTION;
    
    // Calcular tensão real considerando divisor de tensão
    // Ajuste esta fórmula conforme seu circuito real
    return measuredVoltage;
}

void sendMeasurements(float current, float voltage) {
    char buffer[128];
    // Formatando os valores com 2 casas decimais
    snprintf(buffer, sizeof(buffer), 
             "Corrente: %.2f A, Tensao: %.2f V\r\n", 
             current, voltage);
    sendString(buffer, &huart1);
}