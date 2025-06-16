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
_FOSCSEL(FNOSC_FRCPLL); 
// Habilita mudanca de clock e configura oscilador
_FOSC(FCKSM_CSECMD & OSCIOFNC_OFF & POSCMD_NONE);

// Definicoes de constantes
#define FCY 40000000           // Frequencia do sistema (40 MHz)
#define FPWM 4000              // Frequencia PWM desejada (4 kHz)
#define PRESCALER 1            // Prescaler do PWM
#define PERIOD (FCY/(FPWM*PRESCALER)-1)  // Calculo do periodo do PWM
#define DUTY_CYCLE(percent) ((uint16_t)((2 * PERIOD * (percent)) / 100))  // Calculo do duty cycle

#define ADC_RESOLUTION 1023    // Resolucao do ADC (10 bits)

#define MAX DUTY_CYCLE(100)    // Valor maximo de duty cycle (100%)
#define MIN DUTY_CYCLE(0)      // Valor minimo de duty cycle (0%)

#define ADC_TO_DUTY(adc_val) ((uint16_t)(((uint32_t)(adc_val) * MAX) / ADC_RESOLUTION))  // Conversao ADC para duty cycle

#define CH_LM35 2              // Canal ADC AN2 sensor LM35 (temperatura)
#define CH_LDR  3              // Canal ADC AN3 para LDR (luminosidade)

// Variaveis globais
uint16_t TEMPERATURE;          // Armazena leitura de temperatura
uint16_t LIGHT;                // Armazena leitura de luminosidade
uint8_t EMERGENCY = 0;         // Flag de emergencia

// Prototipos de funcoes
void PLL_Init(void);           // Inicializa PLL
void GPIO_Init(void);          // Configura GPIOs
void INT0_Init(void);          // Configura interrupcao externa
void AD_Init(void);            // Inicializa ADC
int ADC_Read(uint8_t channel); // Le canal ADC
void MCPWM_Init(void);         // Inicializa modulo PWM
void runFAN(uint16_t LM35);    // Controla velocidade do ventilador
void setLIGHT(uint16_t LDR);   // Controla intensidade da luz

// Funcao principal
int main(void) {
    PLL_Init();       // Inicializa PLL para 40 MHz
    GPIO_Init();      // Configura pinos
    INT0_Init();      // Configura interrupcao
    MCPWM_Init();     // Inicializa PWM
    AD_Init();        // Inicializa ADC
    
    while (1) { 
      if (!EMERGENCY) {  // Modo normal de operacao
        TEMPERATURE = ADC_Read(CH_LM35);  // Le temperatura
        LIGHT = ADC_Read(CH_LDR);         // Le luminosidade
        runFAN(TEMPERATURE);              // Controla ventilador
        setLIGHT(LIGHT);                  // Controla luz
      }
      else{  // Modo emergencia
        runFAN(0);      // Desliga ventilador
        setLIGHT(0);    // Desliga luz
      }
    }
    return 0;
}

// Tratador de interrupcao INT0 (botao de emergencia)
void __attribute__((interrupt, auto_psv)) _INT0Interrupt(void)
{
    EMERGENCY = !EMERGENCY;  // Alterna estado de emergencia
    IFS0bits.INT0IF = 0;     // Limpa flag de interrupcao
}

// Inicializa PLL para 40 MHz
void PLL_Init(void)
{
    PLLFBD = 41;             // M = 43
    CLKDIVbits.PLLPOST = 0;  // N1 = 2
    CLKDIVbits.PLLPRE = 0;   // N2 = 2
    while (OSCCONbits.COSC != 0b001);  // Aguarda PLL travar
};

// Configura pinos GPIO
void GPIO_Init(void)
{
    AD1PCFGL = 0xFFFF;       // Todos pinos como digitais inicialmente
    AD1PCFGLbits.PCFG2 = 0;  // AN2 como analogico (LM35)
    AD1PCFGLbits.PCFG3 = 0;  // AN3 como analogico (LDR)
    
    TRISBbits.TRISB0 = 1;    // RB0 como entrada (LM35)
    TRISBbits.TRISB1 = 1;    // RB1 como entrada (LDR)
    
    TRISBbits.TRISB12 = 0;   // RB12 como saida (ILUMINACAO)
    TRISBbits.TRISB14 = 0;   // RB14 como saida (VENTILADOR)
    
    TRISBbits.TRISB7 = 1;    // RB7 como entrada (INT0)
};

// Inicializa modulo PWM
void MCPWM_Init(void)
{
    P1TCONbits.PTEN = 0;        // Desabilita temporizador
    P1TCONbits.PTMOD = 0b00;    // Modo free run (dente de serra)
    P1TCONbits.PTCKPS = 0b00;   // Prescaler 1:1        
    P1TPER = PERIOD;            // Define periodo do PWM
    
    PWM1CON1bits.PEN1H = 1;     // Habilita PWM no pino 1H (VENTILADOR)
    PWM1CON1bits.PEN2H = 1;     // Habilita PWM no pino 2H (ILUMINACAO)
    
    PWM1CON1bits.PMOD1 = 1;     // Modo independente para PWM1
    PWM1CON1bits.PMOD2 = 1;     // Modo independente para PWM2
    
    P1TCONbits.PTEN = 1;        // Habilita temporizador
}

// Inicializa ADC
void AD_Init(void)
{
    AD1CON1 = 0x0000;        // Conversao manual
    AD1CON1bits.FORM = 0b00; // Resultado em inteiro
    AD1CSSL = 0;             // Sem varredura
    AD1CON2 = 0;             // Referencia = AVdd/AVss
    AD1CON3 = 0x0002;        // Tad = 3 Tcy
}

// Le valor do ADC em um canal especifico
int ADC_Read(uint8_t channel)
{
    AD1CON1bits.ADON = 1;        // Liga ADC
    AD1CHS0bits.CH0SA = channel; // Seleciona canal
    AD1CON1bits.SAMP = 1;        // Inicia amostragem
    for(int i = 0; i < 100; i++); // Delay para aquisicao
    AD1CON1bits.SAMP = 0;        // Inicia conversao
    while (!AD1CON1bits.DONE);   // Aguarda fim da conversao
    return ADC1BUF0;             // Retorna valor convertido
}

// Controla velocidade do ventilador baseado na temperatura
void runFAN(uint16_t LM35)
{
    uint16_t duty = ADC_TO_DUTY(LM35);  // Converte leitura ADC para duty cycle
    P1DC1 = duty;                       // Aplica duty cycle no PWM
}

// Controla intensidade da luz baseado na luminosidade
void setLIGHT(uint16_t LDR)
{
    uint16_t duty = ADC_TO_DUTY(LDR);  // Converte leitura ADC para duty cycle
    P1DC2 = duty;                      // Aplica duty cycle no PWM
}

// Configura interrupcao externa INT0 (botao de emergencia)
void INT0_Init(void)
{
    INTCON2bits.INT0EP = 1;    // Interrupcao na borda de descida
    IFS0bits.INT0IF = 0;       // Limpa flag de interrupcao
    IEC0bits.INT0IE = 1;       // Habilita interrupcao INT0
}