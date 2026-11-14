



// DSPIC33EP512GM604 Configuration Bit Settings

// 'C' source line config statements

// FICD
#pragma config ICS = PGD1               // ICD Communication Channel Select bits (Communicate on PGEC1 and PGED1)
#pragma config JTAGEN = OFF             // JTAG Enable bit (JTAG is disabled)

// FPOR
#pragma config BOREN = ON               //  (BOR is enabled)
#pragma config ALTI2C1 = OFF            // Alternate I2C1 pins (I2C1 mapped to SDA1/SCL1 pins)
#pragma config ALTI2C2 = OFF            // Alternate I2C2 pins (I2C2 mapped to SDA2/SCL2 pins)
#pragma config WDTWIN = WIN25           // Watchdog Window Select bits (WDT Window is 25% of WDT period)

// FWDT
#pragma config WDTPOST = PS1024         // Watchdog Timer Postscaler bits (1:32,768)
#pragma config WDTPRE = PR32            // Watchdog Timer Prescaler bit (1:128)
#pragma config PLLKEN = ON              // PLL Lock Enable bit (Clock switch to PLL source will wait until the PLL lock signal is valid.)
#pragma config WINDIS = OFF             // Watchdog Timer Window Enable bit (Watchdog Timer in Non-Window mode)
#pragma config FWDTEN = OFF             // Watchdog Timer Enable bit (Watchdog timer enabled/disabled by user software)

// FOSC
#pragma config POSCMD = XT              // Primary Oscillator Mode Select bits (XT Crystal Oscillator Mode)
#pragma config OSCIOFNC = OFF           // OSC2 Pin Function bit (OSC2 is clock output)
#pragma config IOL1WAY = ON             // Peripheral pin select configuration (Allow only one reconfiguration)
#pragma config FCKSM = CSECMD           // Clock Switching Mode bits (Clock switching is enabled,Fail-safe Clock Monitor is disabled)

// FOSCSEL
#pragma config FNOSC = PRI              // Oscillator Source Selection (Primary Oscillator (XT, HS, EC))
#pragma config PWMLOCK = OFF            // PWM Lock Enable bit (PWM registers may be written without key sequence)
#pragma config IESO = ON                // Two-speed Oscillator Start-up Enable bit (Start up device with FRC, then switch to user-selected oscillator source)

// FGS
#pragma config GWRP = OFF               // General Segment Write-Protect bit (General Segment may be written)
#pragma config GCP = OFF                // General Segment Code-Protect bit (General Segment Code protect is Disabled)

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.


#include "xc.h"
#include <libpic30.h> // Needed for delay32

void init_clock_signal(void)
{
    RCONbits.SWDTEN = 0; // Disable Watchdog Timer

    // This chip is taged with I-Temp stamp (-I/SP) then the temperature range
    // is -40ºC to 85ºC which implies a maximum of 70 MIPS (Fosc = 140 MHz)
    // Chapter 33.1 DC characteristics in dsPIC33EP512GM604 manual.
    
    // We take a conservative configuration
    
    // Configure Oscillator to operate the device at 40 Mhz
    // Fosc = Fin*M/(N1*N2), Fcy = Fosc/2
    // Fosc = 7.3728*65/(2*2) = 119.808 MHz
    // Fcy = Fosc/2 = 59.904 MHz

    // Configure PLL prescaler, PLL postscaler and PLL divisor

    PLLFBDbits.PLLDIV = 63; // M = PLLDIV + 2 = 65 -> PLLDIV = 65 - 2 = 63
    CLKDIVbits.PLLPOST = 0; // N2 = 2 (Output/2)
    CLKDIVbits.PLLPRE = 0; // N1 = 2 (Input/2)
    
    // clock switching to incorporate PLL
    __builtin_write_OSCCONH(0x03); // Initiate Clock Switch to Primary
    __builtin_write_OSCCONL(0x01); // Start clock switching

    while (OSCCONbits.COSC != 0b011); // Wait for Clock switch to occur
    while (OSCCONbits.LOCK != 1) {}; // Wait for PLL to lock (If LOCK = 1 -> PLL start-up timer is satisfied)

}



void init_uart1(void)
{
     /* Specified pins for UART1 */
    
    /* RX RPI44 */
    RPINR18bits.U1RXR = 0b0101100;
    TRISBbits.TRISB12 = 1;
    
    /* TX RP43 */
    RPOR4bits.RP43R = 0b000001;
    TRISBbits.TRISB11 = 0;
    
    
    /* Configure UART */
    U1MODEbits.USIDL = 0; //Continues module operation in Idle mode
    U1MODEbits.IREN = 0; //IrDA encoder and decoder are disabled
    U1MODEbits.UEN = 0b00; //UxTX and UxRX pins are enabled and used; UxCTS and UxRTS/BCLKx pins are controlled by PORT latches
    U1MODEbits.WAKE = 0; //No wake-up is enabled
    U1MODEbits.LPBACK = 0; //Loopback mode is disabled
    U1MODEbits.ABAUD = 0; //Baud rate measurement is disabled or has completed
    U1MODEbits.URXINV = 0; //UxRX Idle state is ?1?
    U1MODEbits.BRGH = 0; //BRG generates 16 clocks per bit period (16x baud clock, Standard mode)
    U1MODEbits.PDSEL = 0b00; //8-bit data, no parity
    U1MODEbits.STSEL = 0; //One Stop bit
    
    
    U1STAbits.URXISEL = 0b00; //nterrupt is set when any character is received and transferred from the UxRSR to the receive buffer; receive buffer has one or more characters
    
    // U1BRG = (Fcy/(16*Baud_rate) -1)
    //U1BRG =  (59.904 /(16*9600)-1);           
    U1BRG = 389;  
    
    U1MODEbits.UARTEN = 1; //UARTx is enabled; all UARTx pins are controlled by UARTx as defined by UEN<1:0>
    U1STAbits.UTXEN = 1; //Transmit is enabled, UxTX pin is controlled by UARTx

            
    /* It is needed to wait one transmision bit to ensure start bit detection
       in the connected receiver device.
       When 9600 Baud rate is selected it is necessary to wait 104 us */

    __delay32(5710); 
    
    //__C30_UART = 1; // Redirects printf output to UART1
}

/* ---- Apartado 4, versión CON INTERRUPCIÓN de recepción ---- */

void init_ports(void)
{
    TRISBbits.TRISB8 = 0;   // RB8 como salida (LED de depuración, opcional)
    LATBbits.LATB8 = 0;
}

void init_uart1_rx_interrupt(void)
{
    U1STAbits.URXISEL = 0b00; // Flag cuando llega 1 carácter
    IPC2bits.U1RXIP = 4;      // Prioridad 4 (cualquiera de 1 a 7 vale; 0 = deshabilitada)
    IFS0bits.U1RXIF = 0;      // Borra el flag ANTES de habilitar
    IEC0bits.U1RXIE = 1;      // Habilita la interrupción de RX de la UART1
}

int main(void) {
    init_clock_signal();      // Igual que en los apartados 2 y 3 (con los #pragma de arriba)
    init_ports();
    init_uart1();
    init_uart1_rx_interrupt();

    while (1) {
        // La CPU queda libre: el eco lo hace la interrupción
    }
    return 0;
}

// Nombre EXACTO del vector: _U1RXInterrupt. Ojo: __attribute__ lleva dos guiones bajos delante y detrás
void __attribute__((interrupt(no_auto_psv))) _U1RXInterrupt(void)
{
    LATBbits.LATB8 ^= 1;           // Depuración: el LED cambia cada vez que entra (quítalo si no tienes LED)

    if (U1STAbits.OERR) {          // Si hubo desbordamiento, la UART deja de recibir
        U1STAbits.OERR = 0;        // hasta que se borra OERR
    }

    while (U1STAbits.URXDA) {      // Vacía todo lo recibido
        char dato = U1RXREG;       // Leer U1RXREG saca el carácter del buffer
        while (U1STAbits.UTXBF);   // Espera hueco en el buffer de TX
        U1TXREG = dato;            // Eco
    }

    IFS0bits.U1RXIF = 0;           // Borra el flag al final
}
