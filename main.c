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

#define FCY 40000000
#define FPWM 4000
#define PRESCALER 1
#define PERIOD (FCY/(FPWM*PRESCALER)-1) 
#define DUTY_CYCLE(percent) ((uint16_t)((2 * PERIOD * (percent)) / 100))

#define MAX DUTY_CYCLE(95)  // 19998
#define MIN DUTY_CYCLE(5)   // 999 

#define B1 PORTBbits.RB0
#define B2 PORTBbits.RB1
#define B3 PORTBbits.RB2
#define B4 PORTBbits.RB3

//#define MAX 18998
//#define MIN 999

// Prototipo de funcoes
void PLL_Init(void);
void GPIO_Init(void);
void MCPWM_Init(void);
void setRGB(void);
unsigned int i;

int main(void) {
    PLL_Init();
    GPIO_Init();
    MCPWM_Init();
            
    while (1) { 
        setRGB();
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
    // Modo de entrada dos botoes
    TRISBbits.TRISB0 = 1;
    TRISBbits.TRISB1 = 1;
    TRISBbits.TRISB2 = 1;
    TRISBbits.TRISB3 = 1;
    // Desabilita analogico
    AD1PCFGL = 0xFFFF;
    // Modo de saida do LED
    TRISBbits.TRISB10 = 0; 
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
    // Habilitar o perif�rico no pino I/O
    PWM1CON1bits.PEN1H = 1;
    PWM1CON1bits.PEN2H = 1;
    PWM1CON1bits.PEN3H = 1;
    // Modo independente
    PWM1CON1bits.PMOD1 = 1;
    PWM1CON1bits.PMOD2 = 1;
    PWM1CON1bits.PMOD3 = 1;
    
    P1TCONbits.PTEN = 1;
}

void setRGB()
{
   if(B1){          // AMARELO
     P1DC1 = MAX;
     P1DC2 = MAX;
     P1DC3 = MIN;
   }  
   else if(B2){     // VERDE
     P1DC1 = MIN;
     P1DC2 = MAX;
     P1DC3 = MIN;
   }
   else if(B3){     // AZUL
     P1DC1 = MIN;
     P1DC2 = MIN;
     P1DC3 = MAX;
   }
   else if(B4){     // BRANCO
     P1DC1 = MAX;
     P1DC2 = MAX;
     P1DC3 = MAX;
   }
}
