#include <EFM8LB1.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SYSCLK      72000000L  // SYSCLK frequency in Hz
#define BAUDRATE      115200L  // Baud rate of UART in bps

unsigned char overflow_count;

char _c51_external_startup (void)
{
    // Disable Watchdog with key sequence
    SFRPAGE = 0x00;
    WDTCN = 0xDE; //First key
    WDTCN = 0xAD; //Second key
    VDM0CN |= 0x80;
    RSTSRC = 0x02;

#if (SYSCLK == 48000000L)	
    SFRPAGE = 0x10;
		PFE0CN  = 0x10; // SYSCLK < 50 MHz.
		SFRPAGE = 0x00;
#elif (SYSCLK == 72000000L)
    SFRPAGE = 0x10;
    PFE0CN  = 0x20; // SYSCLK < 75 MHz.
    SFRPAGE = 0x00;
#endif

#if (SYSCLK == 12250000L)
    CLKSEL = 0x10;
		CLKSEL = 0x10;
		while ((CLKSEL & 0x80) == 0);
#elif (SYSCLK == 24500000L)
    CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
#elif (SYSCLK == 48000000L)
    // Before setting clock to 48 MHz, must transition to 24.5 MHz first
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
		CLKSEL = 0x07;
		CLKSEL = 0x07;
		while ((CLKSEL & 0x80) == 0);
#elif (SYSCLK == 72000000L)
    // Before setting clock to 72 MHz, must transition to 24.5 MHz first
    CLKSEL = 0x00;
    CLKSEL = 0x00;
    while ((CLKSEL & 0x80) == 0);
    CLKSEL = 0x03;
    CLKSEL = 0x03;
    while ((CLKSEL & 0x80) == 0);
#else
#error SYSCLK must be either 12250000L, 24500000L, 48000000L, or 72000000L
#endif

    P0MDOUT |= 0x10; // Enable UART0 TX as push-pull output
    XBR0     = 0x01; // Enable UART0 on P0.4(TX) and P0.5(RX)
    XBR1     = 0X00;
    XBR2     = 0x40; // Enable crossbar and weak pull-ups

#if (((SYSCLK/BAUDRATE)/(2L*12L))>0xFFL)
#error Timer 0 reload value is incorrect because (SYSCLK/BAUDRATE)/(2L*12L) > 0xFF
#endif
    // Configure Uart 0
    SCON0 = 0x10;
    CKCON0 |= 0b_0000_0000 ; // Timer 1 uses the system clock divided by 12.
    TH1 = 0x100-((SYSCLK/BAUDRATE)/(2L*12L));
    TL1 = TH1;      // Init Timer1
    TMOD &= ~0xf0;  // TMOD: timer 1 in 8-bit auto-reload
    TMOD |=  0x20;
    TR1 = 1; // START Timer1
    TI = 1;  // Indicate TX0 ready

    return 0;
}

// Uses Timer3 to delay <us> micro-seconds. 
void Timer3us(unsigned char us)
{
    unsigned char i;               // usec counter

    // The input for Timer 3 is selected as SYSCLK by setting T3ML (bit 6) of CKCON0:
    CKCON0|=0b_0100_0000;

    TMR3RL = (-(SYSCLK)/1000000L); // Set Timer3 to overflow in 1us.
    TMR3 = TMR3RL;                 // Initialize Timer3 for first overflow

    TMR3CN0 = 0x04;                 // Sart Timer3 and clear overflow flag
    for (i = 0; i < us; i++)       // Count <us> overflows
    {
        while (!(TMR3CN0 & 0x80));  // Wait for overflow
        TMR3CN0 &= ~(0x80);         // Clear overflow indicator
    }
    TMR3CN0 = 0 ;                   // Stop Timer3 and clear overflow flag
}

void waitms (unsigned int ms)
{
    unsigned int j;
    for(j=ms; j!=0; j--)
    {
        Timer3us(249);
        Timer3us(249);
        Timer3us(249);
        Timer3us(250);
    }
}

void InitADC (void)
{
    SFRPAGE = 0x00;
    ADC0CN1 = 0b_10_000_000; //14-bit,  Right justified no shifting applied, perform and Accumulate 1 conversion.
    ADC0CF0 = 0b_11111_0_00; // SYSCLK/32
    ADC0CF1 = 0b_0_0_011110; // Same as default for now
    ADC0CN0 = 0b_0_0_0_0_0_00_0; // Same as default for now
    ADC0CF2 = 0b_0_01_11111 ; // GND pin, Vref=3.3035
    ADC0CN2 = 0b_0_000_0000;  // Same as default for now. ADC0 conversion initiated on write of 1 to ADBUSY.
    ADEN=1; // Enable ADC
}

void InitPinADC (unsigned char portno, unsigned char pin_num)
{
    unsigned char mask;

    mask=1<<pin_num;

    SFRPAGE = 0x20;
    switch (portno)
    {
        case 0:
            P0MDIN &= (~mask); // Set pin as analog input
            P0SKIP |= mask; // Skip Crossbar decoding for this pin
            break;
        case 1:
            P1MDIN &= (~mask); // Set pin as analog input
            P1SKIP |= mask; // Skip Crossbar decoding for this pin
            break;
        case 2:
            P2MDIN &= (~mask); // Set pin as analog input
            P2SKIP |= mask; // Skip Crossbar decoding for this pin
            break;
        default:
            break;
    }
    SFRPAGE = 0x00;
}

unsigned int ADC_at_Pin(unsigned char pin)
{
    ADC0MX = pin;   // Select input from pin
    ADBUSY=1;       // Dummy conversion first to select new pin
    while (ADBUSY); // Wait for dummy conversion to finish
    ADBUSY = 1;     // Convert voltage at the pin
    while (ADBUSY); // Wait for conversion to complete
    return (ADC0);
}


float Volts_at_Pin(unsigned char pin)
{
    return ((ADC_at_Pin(pin)*3.3035)/16383.0);
}

void TIMER0_Init(void)
{
    TMOD&=0b_1111_0000; // Set the bits of Timer/Counter 0 to zero
    TMOD|=0b_0000_0001; // Timer/Counter 0 used as a 16-bit timer
    TR0=0; // Stop Timer/Counter 0
}

void LCD_pulse (void)
{
    P2_5=1;
    Timer3us(40);
    P2_5=0;
}

void LCD_byte (unsigned char x)
{
    // The accumulator in the C8051Fxxx is bit addressable!
    ACC=x; //Send high nible
    P2_1=ACC_7;
    P2_2=ACC_6;
    P2_3=ACC_5;
    P2_4=ACC_4;
    LCD_pulse();
    Timer3us(40);
    ACC=x; //Send low nible
    P2_1=ACC_3;
    P2_2=ACC_2;
    P2_3=ACC_1;
    P2_4=ACC_0;
    LCD_pulse();
}

void WriteData (unsigned char x)
{
    P2_6=1;
    LCD_byte(x);
    waitms(2);
}

void WriteCommand (unsigned char x)
{
    P2_6=0;
    LCD_byte(x);
    waitms(5);
}

void LCD_4BIT (void)
{
    P2_5=0; // Resting state of LCD's enable is zero
    // LCD_RW=0; // We are only writing to the LCD in this program
    waitms(20);
    // First make sure the LCD is in 8-bit mode and then change to 4-bit mode
    WriteCommand(0x33);
    WriteCommand(0x33);
    WriteCommand(0x32); // Change to 4-bit mode

    // Configure the LCD
    WriteCommand(0x28);
    WriteCommand(0x0c);
    WriteCommand(0x01); // Clear screen command (takes some time)
    waitms(20); // Wait for clear screen command to finish.
}

void LCDprint(char * string, unsigned char line, bit clear)
{
    int j;
    WriteCommand(line==2?0xc0:0x80);
    waitms(5);
    for(j=0; string[j]!=0; j++)	WriteData(string[j]);// Write the message
    if(clear) for(; j<16; j++) WriteData(' '); // Clear the rest of the line
}

// returns max RMS voltage of P1.7 or P1.6
// pin == 1 means P1.7, pin == 0 meeans P1.6
float getAmplitude(float vRMS, int pin) {
    float vTemp = 0;
    // check which pin
    if (pin == 1) {
        vTemp = Volts_at_Pin(QFP32_MUX_P1_7);
    }
    else {
        vTemp = Volts_at_Pin(QFP32_MUX_P1_6);
    }

    // check if voltage at pin is greater than current peak voltage
    if (vTemp > (vRMS*1.4142135)) {
        return vTemp / 1.4142135; // returns vRMS of current voltage
    }
    return vRMS;
}

void main (void)
{
    // variables
    float halfPeriod;
    float timeDiff;
    short int phase;
    float v1RMS = 0;
    float v2RMS = 0;

    // strings for LCD  
    char buff1[17] = "";
    char buff2[17] = "";

    char vHolder[5]; // 3 digits, 1 null
    //char v2Holder[5];
    char phaseHolder[5]; // 3 digits, 1 null
    //char freqHolder[5]; // 3 digits, 1 null

    TIMER0_Init();

    LCD_4BIT();

    //waitms(500); // Give PuTTY a chance to start.
    //printf("\x1b[2J"); // Clear screen using ANSI escape sequence.

    InitPinADC(1, 6); // Configure P1.6 as analog input
    InitPinADC(1, 7); // Configure P1.7 as analog input
    InitADC();

    while (1) {
        // Measure halfPeriod of reference/input signals ======================================

        // Reset the counter
        TL0=0;
        TH0=0;
        TF0=0;
        overflow_count=0;

        while(P1_4!=0) { // Wait for the signal to be zero
            // Measure Amplitude ==========================================================
            v1RMS = getAmplitude(v1RMS, 1); // v1RMS = getAmplitude_P1_7(v1RMS);
            v2RMS = getAmplitude(v2RMS, 0); // v2RMS = getAmplitude_P1_7(v2RMS);
        }
        while(P1_4!=1) { // Wait for the signal to be one
            // Measure Amplitude ==========================================================
            v1RMS = getAmplitude(v1RMS, 1); // v1RMS = getAmplitude_P1_7(v1RMS);
            v2RMS = getAmplitude(v2RMS, 0); // v2RMS = getAmplitude_P1_7(v2RMS);
        }
        TR0=1; // Start the timer
        while(P1_4!=0) { // Wait for the signal to be zero
            // Measure Amplitude ==========================================================
            v1RMS = getAmplitude(v1RMS, 1); // v1RMS = getAmplitude_P1_7(v1RMS);
            v2RMS = getAmplitude(v2RMS, 0); // v2RMS = getAmplitude_P1_7(v2RMS);

            if(TF0==1) // Did the 16-bit timer overflow?
            {
                TF0=0;
                overflow_count++;
            }

            // Measure Amplitude ==========================================================
            v1RMS = getAmplitude(v1RMS, 1); // v1RMS = getAmplitude_P1_7(v1RMS);
            v2RMS = getAmplitude(v2RMS, 0); // v2RMS = getAmplitude_P1_7(v2RMS);
        }
        TR0=0; // Stop timer 0, the 24-bit number [overflow_count-TH0-TL0] has the halfPeriod!
        halfPeriod=(overflow_count*65536.0+TH0*256.0+TL0)*(12.0/SYSCLK);

        // Measure time difference signal ========================================================

        // Reset the counter
        TL0=0;
        TH0=0;
        TF0=0;
        overflow_count=0;

        while(P1_5!=0) { // Wait for the signal to be zero
            // Measure Amplitude ==========================================================
            v1RMS = getAmplitude(v1RMS, 1); // v1RMS = getAmplitude_P1_7(v1RMS);
            v2RMS = getAmplitude(v2RMS, 0); // v2RMS = getAmplitude_P1_7(v2RMS);
        }
        while(P1_5!=1) {  // Wait for the signal to be one
            // Measure Amplitude ==========================================================
            v1RMS = getAmplitude(v1RMS, 1); // v1RMS = getAmplitude_P1_7(v1RMS);
            v2RMS = getAmplitude(v2RMS, 0); // v2RMS = getAmplitude_P1_7(v2RMS);
        }
        TR0=1; // Start the timer
        while(P1_5!=0) { // Wait for the signal to be zero
            // Measure Amplitude ==========================================================
            v1RMS = getAmplitude(v1RMS, 1); // v1RMS = getAmplitude_P1_7(v1RMS);
            v2RMS = getAmplitude(v2RMS, 0); // v2RMS = getAmplitude_P1_7(v2RMS);

            if(TF0==1) // Did the 16-bit timer overflow?
            {
                TF0=0;
                overflow_count++;
            }

            // Measure Amplitude ==========================================================
            v1RMS = getAmplitude(v1RMS, 1); // v1RMS = getAmplitude_P1_7(v1RMS);
            v2RMS = getAmplitude(v2RMS, 0); // v2RMS = getAmplitude_P1_7(v2RMS);
        }
        TR0=0; // Stop timer 0, the 24-bit number [overflow_count-TH0-TL0] has the halfPeriod!
        timeDiff=(overflow_count*65536.0+TH0*256.0+TL0)*(12.0/SYSCLK);

        if (1000.0*timeDiff < 0.250) { // if the timeDiff signal is too small, just say the signals are in phase
            phase = 0;
        }
        else {
            if (P1_4 == 0){
                phase = 9 + (1)*((timeDiff*180) / (halfPeriod));
            } else {
                phase = (-1)*((timeDiff*180) / (halfPeriod)) - 9;
            }
        }

        if (abs(phase) > 50){
            P3_0 = 1;
        } else {
            P3_0 = 0;
        }

        // Send values to putty ================================================================
        printf("\rP1.6 %.2f V   P1.7 %.2f V   T=%.2f ms   phase=%d degrees", v2RMS, v1RMS, halfPeriod*2*1000.0, phase);

        // Send values to LCD =================================================================
        strcpy(buff1, "V1:");
        strcpy(buff2, "V2:");


        if (P1_2 != 0){                 // Pin 1.2 controls the voltage, if pressed displays peak
            sprintf(vHolder, "%.2f", v1RMS);
        } else {
            sprintf(vHolder, "%.2f", v1RMS*1.4142135);
        }
        strncat(buff1, vHolder, 5);
        strncat(buff1, "V ", 3);





        if (P1_2 != 0){                 // Pin 1.2 controls the voltage, if pressed displays peak
            sprintf(vHolder, "%.2f", v2RMS);
        } else {
            sprintf(vHolder, "%.2f", v2RMS*1.4142135);
        }
        strncat(buff2, vHolder, 5);
        strncat(buff2, "V ", 3);




        if (P1_1 != 0){                 // Pin 1.1 controls the frequency, if pressed displays period
            strncat(buff1, "F=", 3);
            sprintf(phaseHolder, "%.0f", 1/(2*halfPeriod));
            strncat(buff1, phaseHolder, 5);
            strncat(buff1, "Hz", 3);
        } else {
            strncat(buff1, "T=", 3);
            sprintf(phaseHolder, "%.0f", 2*halfPeriod*1000);
            strncat(buff1, phaseHolder, 5);
            strncat(buff1, "ms", 3);
        }




        strncat(buff2, "P=", 3);
        if (P1_3 == 0){                 // Pin 1.3 controls the phase, if pressed displays radians
            sprintf(phaseHolder, "%.2f", ((float)phase*3.141592/180));
            strncat(buff2, phaseHolder, 5);
            strncat(buff2, "r", 2);
        } else {
            sprintf(phaseHolder, "%d", phase);
            strncat(buff2, phaseHolder, 5);
            strncat(buff2, "d", 2);
        }


        

        LCDprint(buff1, 1, 1);
        LCDprint(buff2, 2, 1);

        //waitms(500);
    }
}