#include <msp430.h>
#include <stdbool.h>
#include <driverlib.h>

const unsigned char LCD_Num[10] = {0xFC, 0x60, 0xDB, 0xF3, 0x67, 0xB7, 0xBF, 0xE0, 0xFF, 0xE7};

volatile unsigned char S1buttonDebounce = 0;
volatile unsigned char S2buttonDebounce = 0;

void Initialize_LCD();
void display_num_lcd(unsigned int n);
void config_ACLK_to_32KHz_crystal();

main()
{
    WDTCTL = WDTPW | WDTHOLD;   // stop watchdog timer
    PM5CTL0 = 0xFFFE;

    // TIMER0
    TA0CCR0 = 32768; // Pone el valor de captura del Timer0 a 32768 (T=1s) (fuente de interrupcion provocada al comparar)
    TA0CTL = 0x0100 + 0x0010; // Pone el ACLK (0x0100) en modo UP (0x0010)
    TA0CCTL0 = CCIE; // Habilita las interrupciones para el timer0

    // TIMER1
    TA1CCR0 = 3276; // Pone el valor de captura del Timer0 a 3276 (T=100ms) (fuente de interrupcion provocada al comparar)
    TA1CTL = TASSEL_1 | TACLR | MC_0;
    TA1CCTL0 = CCIE; // Habilita las interrupciones para el timer1

    //TIMER2 (Usado como debouncer)
    TA2CCR0 = 492; // Pone el valor de captura del Timer2 a 492 (T=15ms)
    TA2CTL = TASSEL_1 | TACLR | MC_0;
    TA2CCTL0 = CCIE; // Habilita las interrupciones para el timer2

    // Interrupciones
    _BIS_SR(GIE);       // Activa las interrupciones que ya estaban habilitadas

    // Pines de entrada y salida

    P1DIR &= ~(BIT1);   // Habilita el pin1
    P1REN |=  BIT1;     // Resistor enable
    P1IFG &= ~BIT1;     // Clear registro de interrupciones
    P1IE  |= BIT1;      // Habilita interrupciones
    P1IES |= BIT1;      // Selecciona flanco de bajada


    P1DIR &= ~(BIT2);   // Habilita el pin2
    P1REN |=  BIT2;     // Resistor enable
    P1IFG &= ~BIT2;     // Clear registro de interrupciones
    P1IE  |= BIT2;      // Habilita interrupciones
    P1IES |= BIT2;      // Selecciona flanco de bajada


    P1SEL0 &= ~(PASEL0_L);  //
    P1SEL1 &= ~(PASEL1_L);  //
    P1DIR |= BIT0;          // Habilita el puerto1 pin0

    P9SEL0 &= ~(PESEL0_L);  //
    P9SEL1 &= ~(PASEL1_L);  //
    P9DIR |= BIT7;          // Habilita el puerto9 pin7


    config_ACLK_to_32KHz_crystal();
    Initialize_LCD();

    // Interrupciones
    _BIS_SR(LPM4_bits + GIE);
}

volatile bool fastCounter = false;
unsigned int contador = 0;

#pragma vector = TIMER0_A0_VECTOR
__interrupt void Timer0_ISR (void){
    display_num_lcd(contador);
    contador++;
}

unsigned int contadorF = 0;

#pragma vector = TIMER1_A0_VECTOR
__interrupt void Timer1_ISR (void){
    display_num_lcd(contadorF);
    contadorF++;
}

#pragma vector = PORT1_VECTOR
__interrupt void interruptions (void)
{

    if((P1IFG & BIT1)){ // Button S1 Pressed
        P1OUT |= BIT0;                  // Turns on the led
        if(S1buttonDebounce == 0){
            S1buttonDebounce = 1;       // Set debounce flag on first high to low transition
            TA0CTL ^= BIT4;             // Switches the timer0
            TA1CTL ^= BIT4;             // Switches the timer1
            fastCounter = !fastCounter; // Changes counting mode
            TA2CTL |= MC_1;             // Starts the debounce timer
        }
        P1IFG &= ~BIT1;                 // Clears the interrupt flag
    }

    if((P1IFG & BIT2)){
        P9OUT |= BIT7;                 // Turns on the led
        if(S2buttonDebounce == 0){
            S2buttonDebounce = 1;       // Set debounce flag on first high to low transition
            TA2CTL |= MC_1;             // Starts the debounce timer
            if(fastCounter){            // if its counting fast
                TA1CTL |= TACLR;        // Clears the timer register
                contadorF = 0;          // Sets the counter 0
            }
            else{
                TA0CTL |= TACLR;
                contador = 0;
            }
        }
        P1IFG &= ~BIT2;                 // Clears the interrupt flag
    }

}

/*
 * Timer A2 Interrupt Service Routine
 * Used as button debounce timer
 */
#pragma vector = TIMER2_A0_VECTOR
__interrupt void TIMER2_ISR (void)
{

    // Button S1 released
    if (P1IN & BIT1)
    {
        S1buttonDebounce = 0;               // Clear button debounce
        P1OUT &= ~BIT0;                     // Turns off the led
    }

    // Button S2 released
    if (P1IN & BIT2)
    {
        S2buttonDebounce = 0;               // Clear button debounce
        P9OUT &= ~BIT7;                     // Turns off the led
    }

    // Both button S1 & S2 released
    if ((P1IN & BIT1) && (P1IN & BIT2)){
        TA2CTL &= ~(MC_3 | TACLR);          // Stops and clears the debounce timer
    }
}


//**********************************************************
// Initializes the LCD_C module
// *** Source: Function obtained from MSP430FR6989s Sample Code***
void Initialize_LCD() {
    PJSEL0 = BIT4 | BIT5; // For LFXT
    // Initialize LCD segments 0 - 21; 26 - 43
    LCDCPCTL0 = 0xFFFF;
    LCDCPCTL1 = 0xFC3F;
    LCDCPCTL2 = 0x0FFF;
    // Configure LFXT 32kHz crystal
    CSCTL0_H = CSKEY >> 8; // Unlock CS registers
    CSCTL4 &= ~LFXTOFF; // Enable LFXT
    do {
        CSCTL5 &= ~LFXTOFFG; // Clear LFXT fault flag
        SFRIFG1 &= ~OFIFG;
    }while (SFRIFG1 & OFIFG); // Test oscillator fault flag
    CSCTL0_H = 0; // Lock CS registers
    // Initialize LCD_C
    // ACLK, Divider = 1, Pre-divider = 16; 4-pin MUX
    LCDCCTL0 = LCDDIV__1 | LCDPRE__16 | LCD4MUX | LCDLP;
    // VLCD generated internally,
    // V2-V4 generated internally, v5 to ground
    // Set VLCD voltage to 2.60v
    // Enable charge pump and select internal reference for it
    LCDCVCTL = VLCD_1 | VLCDREF_0 | LCDCPEN;
    LCDCCPCTL = LCDCPCLKSYNC; // Clock synchronization enabled
    LCDCMEMCTL = LCDCLRM; // Clear LCD memory
    //Turn LCD on
    LCDCCTL0 |= LCDON;
    return;
}



//***************function that displays any 16-bit unsigned integer************
void display_num_lcd(unsigned int n){
    //initialize i to count though input paremter from main function, digit is used for while loop so as long as n is not 0 the if statements will be executed.

    int i;
    int digit;
    digit = n;
    while(digit!=0){
        digit = digit*10;
        i++;
    }
    if(i>1000){
        LCDM8 = LCD_Num[n%10];
        n=n/10;
        i++;
    }
    if(i>100){
        LCDM15 = LCD_Num[n%10];
        n=n/10;
        i++;
    }
    if(i>10){
        LCDM19 = LCD_Num[n%10];
        n=n/10;
        i++;
    }
    if(i>1){
        LCDM4 = LCD_Num[n%10];
        n=n/10;
        i++;
    }
    if(i>0){
        LCDM6 = LCD_Num[n%10];
        n=n/10;
        i++;
    }

}

//**********************************
// Configures ACLK to 32 KHz crystal
void config_ACLK_to_32KHz_crystal() {
    // By default, ACLK runs on LFMODCLK at 5MHz/128 = 39 KHz
    // Reroute pins to LFXIN/LFXOUT functionality
    PJSEL1 &= ~BIT4;
    PJSEL0 |= BIT4;
    // Wait until the oscillator fault flags remain cleared
    CSCTL0 = CSKEY; // Unlock CS registers
    do {
        CSCTL5 &= ~LFXTOFFG; // Local fault flag
        SFRIFG1 &= ~OFIFG; // Global fault flag
    } while((CSCTL5 & LFXTOFFG) != 0);
    CSCTL0_H = 0; // Lock CS registers
    return;
}

