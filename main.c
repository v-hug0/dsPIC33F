/*
 * Projeto 01 - Controle de Velocidade de Motor DC
 * 
 * Descrição:
 * - Utiliza um potenciômetro conectado em AN0 (RB0) para ajustar o duty cycle do PWM
 * - PWM controla a velocidade de um motor DC
 * - Inclui um botão em RB4 para simular a função de freio
 * - Quando solto, retorna ao controle pelo potenciômetro
 * 
 * Configurações:
 * - Clock do sistema: 40MHz (FRCPLL com M=43, N1=2, N2=2)
 * - Frequência PWM: 4kHz
 * - Resolução ADC: 10 bits (0-1023)
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

#define FCY 40000000
#define FPWM 4000
#define PRESCALER 1
#define PERIOD (FCY/(FPWM*PRESCALER)-1) 
// Calcula o valor do duty cycle
#define DUTY_CYCLE(percent) ((uint16_t)((2 * PERIOD * (percent)) / 100)) 

#define ADC_RESOLUTION 1023

#define MAX DUTY_CYCLE(100)
#define MIN DUTY_CYCLE(0)
// Converte valor ADC para duty cycle
#define ADC_TO_DUTY(adc_val) ((uint16_t)(((uint32_t)(adc_val) * MAX) / ADC_RESOLUTION))

// Definição dos pinos
#define POT PORTBbits.RB0       // Potenciômetro conectado em RB0/AN0
#define BRAKE_BUTTON PORTBbits.RB4 // Botão de freio conectado em RB4


uint16_t adcValue; // Variável para armazenar o valor lido do ADC

// Protótipos de funções
void PLL_Init(void);      // Configura o PLL para gerar 40MHz
void GPIO_Init(void);     // Configura os pinos de I/O
void TIMER_Init(void);    // Configura temporizador (não usado neste código)
void AD_Init(void);       // Configura o módulo ADC
int ADC_start(void);      // Inicia conversão ADC e retorna valor
void MCPWM_Init(void);    // Configura o módulo PWM
void motorRun(uint16_t speed); // Controla o motor com velocidade variável
void motorBrake(void);    // Aciona o freio do motor
unsigned int i;

int main(void) {
    PLL_Init();
    GPIO_Init();
    MCPWM_Init();
    AD_Init();   
    while (1) { 
        adcValue = ADC_start();
        if(BRAKE_BUTTON==0){
            motorRun(adcValue);
        } else{
            motorBrake();
        } 
    }
    return 0;
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
    // Modo de entrada do potenciometro e dos botoes
    TRISBbits.TRISB0 = 1;
    TRISBbits.TRISB4 = 1;
    // Habilita o analï¿½gico apenas em RB0
    AD1PCFGL = 0xFFFF;
    AD1PCFGLbits.PCFG0 = 0;
    // Modo de saida do LED 
    TRISBbits.TRISB12 = 0;
    TRISBbits.TRISB14 = 0;
};

void MCPWM_Init(void)
{
    // Base de tempo
    P1TCONbits.PTEN = 0;        
    P1TCONbits.PTMOD = 0b00;    // modo free run (dente de serra) - edge al
    P1TCONbits.PTCKPS = 0b00;   // prescaler 1:1        
    // Periodo do PWM
    P1TPER = PERIOD;
    // Habilitar o periferico no pino I/O
    PWM1CON1bits.PEN1H = 1;
    PWM1CON1bits.PEN2H = 1;
    // Modo independente
    PWM1CON1bits.PMOD1 = 1;
    PWM1CON1bits.PMOD2 = 1;
    
    P1TCONbits.PTEN = 1;
}

void AD_Init(void)
{
    AD1CON1 = 0x0000;   // SAMP bit = 0 ends sampling
                        // and starts converting
    AD1CHS0 = 0x0002;   // Connect RB2/AN2 as CH0 input
    AD1CSSL = 0;        
    AD1CON3 = 0x0002;   // Manual sample, Tad = internal 3 Tcy
    AD1CON2 = 0;        
    
}

int ADC_start(void)
{
    int adc = 0;
    AD1CON1bits.ADON = 1; // Turn ADC ON
    AD1CON1bits.SAMP = 1; // starts sampling
    for(int i = 0; i < 100; i++);
    AD1CON1bits.SAMP = 0; // start converting
    while (!AD1CON1bits.DONE);
    adc = ADC1BUF0;
    return adc;
}

void motorRun(uint16_t speed)
{
    uint16_t duty = ADC_TO_DUTY(speed);
    P1DC1 = duty;
    P1DC2 = MIN;
}

void motorBrake(void){
    P1DC1 = MAX;
    P1DC2 = MAX;
}

