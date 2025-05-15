/*
 * Trabalho 1 de Técnicas Avançadas em Microcontroladores
 * 
 * Autores:
 *      Victor Hugo Silva Maciel - 497553
 *      Eduardo Vilas Boas Simões - 509925
 *
 */

#include "xc.h"
#include "p33FJ12MC202.h"


// Definição de variáveis
int cont = 0; // Auxiliar para Timer 1
int cont2 = 0; // Auxiliar para display de 7 seg
int toggle_flag = 0;  // Flag para botão de pausa

const int display_7seg[10] = {
    0b00111111, // 0
    0b00000110, // 1
    0b01011011, // 2
    0b01001111, // 3
    0b01100110, // 4
    0b01101101, // 5
    0b01111101, // 6
    0b00000111, // 7
    0b01111111, // 8
    0b01101111  // 9
};

#define DISPLAY(n)  (LATB = (LATB & 0xFF00) | display_7seg[(n)])

// Configuração do clock - FRC + PLL
_FOSCSEL(FNOSC_FRCPLL);         // Usa FRC com PLL após o reset
_FOSC(FCKSM_CSECMD & OSCIOFNC_OFF & POSCMD_NONE);  // Nenhum oscilador externo usado


// Protótipo de funções
void PLL_Init(void);
void TIMER_Init(void);
void GPIO_Init(void);
void CN_Init(void);


int main(void) {
    PLL_Init(); // Configuração do oscilador interno FTC com PLL
    TIMER_Init(); // Configuração de Timer 1
    GPIO_Init(); // Configuração de PORTB
    CN_Init(); // Configuração de Change Notification
    while(1);
    return 0;
}

void PLL_Init(void){
    PLLFBD = 41; // M = 43
    CLKDIVbits.PLLPOST=0; // N1 = 2
    CLKDIVbits.PLLPRE=0; // N2 = 2
    while (OSCCONbits.COSC != 0b001);
};

void TIMER_Init(void){
    T1CONbits.TON = 0; // Timer desabilitado para configuração
    T1CONbits.TCS = 0; // Seleciona-se Fcy (Fosc/2) como fonte de clock para o Timer1
    T1CONbits.TGATE = 0; // Modo Gate desabilitado
    T1CONbits.TCKPS = 0b11; // Seleção de 1:256 de prescaler
    TMR1 = 0x00;
    PR1  = 15625;
     /* ---- Cálculo do período de interrupção:----
      Fcy = 40 MHz
      Prescaler (PRE) = 256
      PR1 = 15625
      
      Fint_T1 = Fcy / (PRE * PR1)
              = 40.000.000 / (256 * 15625)
              = 40.000.000 / 4.000.000
              = 10 Hz ? Interrupção a cada 0,1 s (100 ms)   */
    IPC0bits.T1IP = 1; // Prioridade de interrupção
    IFS0bits.T1IF = 0; // Limpar flag de interrupção
    IEC0bits.T1IE = 1; // Habilitar interrupção
    T1CONbits.TON = 1; // Habilitar o Timer1
};

void GPIO_Init(void){
    TRISB = 0;
};

void CN_Init(void){
    CNEN1bits.CN0IE = 1; // Enable CN3 pin for interrupt detection
    //IPC4bits.CNIP = 1;
    IFS1bits.CNIF = 0; // Reset CN interrupt
    IEC1bits.CNIE = 1; // Enable CN interrupts
};


// ISR - Timer 1
void __attribute__((__interrupt__,_auto_psv)) _T1Interrupt(void)
{
	IFS0bits.T1IF = 0;
    
    if(cont<10){
        cont++;   
    } else {
        cont=0;
        if(cont2<10){
            DISPLAY(cont2); // Roda
            cont2++;
        } else {
            cont2 = 0;
        }
    }
};

// ISR - Change Notification
void __attribute__ ((__interrupt__)) _CNInterrupt(void)
{
    // Lógica para controle de pausa com o botão
    if(toggle_flag==0){
        T1CONbits.TON = !T1CONbits.TON; // Interrupção do Timer 1 até o botão ser pressionado novamente
        toggle_flag++;
    } else {
        toggle_flag--;
    }
    IFS1bits.CNIF = 0; // Limpa flag de interrupção CN      
};

