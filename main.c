/*More actions
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

#define MAX DUTY_CYCLE(100)  // 19998
#define MIN DUTY_CYCLE(0)   // 999 

//#define MAX 18998
//#define MIN 999

uint16_t temp;
uint16_t luz;

// Prototipo de funcoes
void PLL_Init(void);
void GPIO_Init(void);
void TIMER_Init(void);
void AD_Init(void);
int ADC_Read(uint8_t channel);
void MCPWM_Init(void);
void CONTROL(uint16_t temp, uint16_t luz);
unsigned int i;

int main(void) {
    PLL_Init();
    GPIO_Init();
    MCPWM_Init();
    AD_Init();   
    while (1) { 
        temp = ADC_Read(0);
        luz = ADC_Read(1);
        CONTROL(luz, temp);
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
    AD1PCFGL = 0xFFFF;            // Todos os pinos como digitais inicialmente
    AD1PCFGLbits.PCFG0 = 0;       // AN0 analógico
    AD1PCFGLbits.PCFG1 = 0;       // AN1 analógico
    TRISAbits.TRISA0 = 1;         // RA0 como entrada (corrente)
    TRISAbits.TRISA1 = 1;         // RA1 como entrada (tensão)
    
    TRISBbits.TRISB12 = 0;  // H2
    TRISBbits.TRISB14 = 0;  // H1
    
        
};

void MCPWM_Init(void)
{
    // Base de tempo
    P1TCONbits.PTEN = 0;        
    P1TCONbits.PTMOD = 0b00;    // modo free run (dente de serra) - edge al
    P1TCONbits.PTCKPS = 0b00;   // prescaler 1:1        
    // Periodo do PWM
    P1TPER = PERIOD;
    // Habilitar o perifï¿½rico no pino I/O
    PWM1CON1bits.PEN1H = 1;
    PWM1CON1bits.PEN2H = 1;
    // Modo independente
    PWM1CON1bits.PMOD1 = 1;
    PWM1CON1bits.PMOD2 = 1;

    P1TCONbits.PTEN = 1;
}

void AD_Init(void)
{
    AD1CON1 = 0x0000;             // Conversão manual
    AD1CON1bits.FORM = 0b00;      // Resultado em formato inteiro
    AD1CSSL = 0;                  // Sem varredura
    AD1CON2 = 0;                  // Ref = AVdd/AVss
    AD1CON3 = 0x0002;             // Tad = 3 Tcy (tempo de aquisição/conversão)

}

int ADC_Read(uint8_t channel)
{
    AD1CON1bits.ADON = 1;              // Liga o ADC
    AD1CHS0bits.CH0SA = channel;       // Seleciona o canal (0 ou 1)
    AD1CON1bits.SAMP = 1;              // Inicia amostragem
    for(int i = 0; i < 100; i++);      // Pequeno delay para aquisição
    AD1CON1bits.SAMP = 0;              // Inicia conversão
    while (!AD1CON1bits.DONE);         // Aguarda fim da conversão
    return ADC1BUF0;                   // Retorna valor convertido
}

void CONTROL(uint16_t temp, uint16_t luz){
//   if((temp > 200) && (temp < 400)){    // AMARELO
//     P1DC1 = DUTY_CYCLE(40);
//   }  
//   else if((temp > 400) && (temp < 600)){    // AMARELO
//     P1DC1 = DUTY_CYCLE(60);
//   }  
//   else if((temp > 600) && (temp < 800)){    // AMARELO
//     P1DC1 = DUTY_CYCLE(80);
//   }  
//   else if((temp > 800)){    // AMARELO
//     P1DC1 = DUTY_CYCLE(100);
//   }  
    
   if(luz <= 200){              // VERMELHO
     P1DC2 = DUTY_CYCLE(100);;
   }
   else if((luz > 200) && (luz < 400)){    // AMARELO
     P1DC2 = DUTY_CYCLE(80);;
   }  
   else if((luz > 400) && (luz < 600)){    // AMARELO
     P1DC2 = DUTY_CYCLE(60);;
   }  
   else if((luz > 600) && (luz < 800)){    // AMARELO
     P1DC2 = DUTY_CYCLE(40);;
   }  
   else if((luz > 800)){    // AMARELO
     P1DC2 = DUTY_CYCLE(20);;
   }   
    
    
}