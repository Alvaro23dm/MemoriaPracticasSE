/*
 * Práctica 4 (CAD) - Apartado 1
 * Toma 3 muestras de tensión por segundo en AN0 y las envía por la USART1.
 *
 * Resumen:
 *  - Reloj: 119.808 MHz (Fcy = 59.904 MHz).
 *  - USART1: 9600 baudios, 8 bits, sin paridad, 1 bit de stop.
 *  - CAD: 12 bits, canal 0 (AN0), disparo manual, conversión por interrupción.
 *  - Un timer avisa cada 1/3 s para tomar una muestra.
 *  - Estructura del programa: la de la guía del profesor (esperar_timer, esperar_ADC).
 */

// DSPIC33EP512GM604 Configuration Bit Settings

// 'C' source line config statements

// FICD
#pragma config ICS = PGD1               // Comunicación con el depurador por PGEC1 y PGED1
#pragma config JTAGEN = OFF             // JTAG desactivado

// FPOR
#pragma config BOREN = ON               // Reset por bajada de tensión activado
#pragma config ALTI2C1 = OFF            // I2C1 en los pines SDA1/SCL1
#pragma config ALTI2C2 = OFF            // I2C2 en los pines SDA2/SCL2
#pragma config WDTWIN = WIN25           // Ventana del watchdog (no se usa)

// FWDT
#pragma config WDTPOST = PS1024         // Postescalado del watchdog (no se usa)
#pragma config WDTPRE = PR32            // Preescalado del watchdog (no se usa)
#pragma config PLLKEN = ON              // El cambio de reloj espera a que la PLL esté enganchada
#pragma config WINDIS = OFF             // Watchdog sin ventana (no se usa)
#pragma config FWDTEN = OFF             // Watchdog desactivado

// FOSC
#pragma config POSCMD = XT              // Oscilador primario: cristal en modo XT
#pragma config OSCIOFNC = OFF           // Función del pin OSC2
#pragma config IOL1WAY = ON             // Los pines remapeables solo se pueden configurar una vez
#pragma config FCKSM = CSECMD           // Cambio de reloj permitido, monitor de fallos desactivado

// FOSCSEL
#pragma config FNOSC = PRI              // Arranca con el oscilador primario (el cristal)
#pragma config PWMLOCK = OFF            // Registros del PWM sin clave de bloqueo
#pragma config IESO = ON                // Arranca con el FRC y luego cambia al oscilador elegido

// FGS
#pragma config GWRP = OFF               // Memoria sin protección contra escritura
#pragma config GCP = OFF                // Memoria sin protección de código

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.


#include "xc.h"
#include <libpic30.h> // Para __delay32
#include <stdio.h>    // Para printf

extern int __C30_UART;  // Indica qué UART usa printf

/* ------------------------------------------------------------------------- */
/*  Reloj                                                                    */
/* ------------------------------------------------------------------------- */

void init_clock_signal(void)
{
    RCONbits.SWDTEN = 0; // Desactiva el watchdog

    // El chip es de temperatura industrial (-40ºC a 85ºC): como máximo 70 MIPS
    // (Fosc = 140 MHz). Se usa una configuración conservadora.

    // Fosc = Fin*M/(N1*N2) y Fcy = Fosc/2
    // Fosc = 7.3728*65/(2*2) = 119.808 MHz
    // Fcy = Fosc/2 = 59.904 MHz

    PLLFBDbits.PLLDIV = 63; // M = PLLDIV + 2 = 65  ->  PLLDIV = 63
    CLKDIVbits.PLLPOST = 0; // N2 = 2
    CLKDIVbits.PLLPRE = 0;  // N1 = 2

    // Cambio de reloj para usar la PLL
    __builtin_write_OSCCONH(0x03); // Pide el oscilador primario con PLL
    __builtin_write_OSCCONL(0x01); // Empieza el cambio

    while (OSCCONbits.COSC != 0b011); // Espera a que el cambio se haga
    while (OSCCONbits.LOCK != 1) {};  // Espera a que la PLL esté enganchada
}

/* ------------------------------------------------------------------------- */
/*  USART1                                                                   */
/* ------------------------------------------------------------------------- */

void init_uart1(void)
{
    /* Pines de la UART1 */

    /* RX en RPI44 (RB12) */
    RPINR18bits.U1RXR = 0b0101100;  // RPI44 = 44
    TRISBbits.TRISB12 = 1;          // Entrada

    /* TX en RP43 (RB11) */
    RPOR4bits.RP43R = 0b000001;     // Código 1 = U1TX
    TRISBbits.TRISB11 = 0;          // Salida


    /* Configuración de la UART: 9600 baudios, 8N1 */
    U1MODEbits.USIDL = 0;           // Sigue funcionando en modo Idle
    U1MODEbits.IREN = 0;            // Sin IrDA
    U1MODEbits.UEN = 0b00;          // Solo se usan los pines TX y RX
    U1MODEbits.WAKE = 0;            // Sin despertar por la línea RX
    U1MODEbits.LPBACK = 0;          // Sin bucle interno
    U1MODEbits.ABAUD = 0;           // Sin autodetección de baudios
    U1MODEbits.URXINV = 0;          // RX en reposo a '1'
    U1MODEbits.BRGH = 0;            // 16 ciclos de reloj por bit
    U1MODEbits.PDSEL = 0b00;        // 8 bits de datos, sin paridad
    U1MODEbits.STSEL = 0;           // 1 bit de stop

    U1STAbits.URXISEL = 0b00;       // Interrupción de RX con cada carácter recibido

    // U1BRG = Fcy/(16*baudios) - 1 = 59904000/(16*9600) - 1 = 389
    U1BRG = 389;

    U1MODEbits.UARTEN = 1;          // Activa la UART
    U1STAbits.UTXEN = 1;            // Activa la transmisión (siempre después de UARTEN)

    /* Hay que esperar un tiempo de bit para que el receptor detecte el bit de start.
       A 9600 baudios es aproximadamente 104 us. */
    __delay32(5710);

    __C30_UART = 1;                 // printf escribe por la UART1
}

/* ------------------------------------------------------------------------- */
/*  CAD                                                                      */
/* ------------------------------------------------------------------------- */

void init_adc(void)
{
    // Pin AN0

    TRISAbits.TRISA0 = 1;   // RA0 como entrada
    // Es una entrada, no una salida digital.

    ANSELAbits.ANSA0 = 1;   // AN0 analógico
    // Si el pin fuera digital, el CAD leería siempre 0.


    AD1CON1bits.AD12B = 1;  // Funcionamiento a 12 bits con un solo canal
    // 12 bits dan 4096 niveles (cada nivel son 0.806 mV). El canal 0 es el único que hay en 12 bits.
    AD1CON1bits.ADSIDL = 1; // El módulo se para si la CPU entra en modo Idle
    // Es el valor de la plantilla. Da igual: este programa nunca entra en Idle.
    AD1CON1bits.FORM = 0;   // Resultado como entero: 0000 dddd dddd dddd
    // El resultado es directamente un número entre 0 y 4095.

    // Poner SAMP a 0 termina el muestreo y empieza la conversión (modo manual)
    AD1CON1bits.SSRCG = 0;
    AD1CON1bits.SSRC = 0;
    // Así el programa decide cuándo empieza la conversión.

    AD1CON1bits.ASAM = 0;   // El muestreo empieza cuando se pone SAMP a 1
    // No es automático: lo empieza el programa.

    // Tensiones de referencia
    AD1CON2bits.VCFG = 0;   // Referencia alta = AVDD, referencia baja = AVSS
    // Así se miden tensiones entre 0 V y 3.3 V.

    AD1CON2bits.CSCNA = 0;  // No se recorren varias entradas
    // Solo se mide AN0.

    // Una interrupción por cada muestra convertida
    AD1CON2bits.SMPI = 0;
    // Cada muestra se envía antes de tomar la siguiente.

    // Modo de llenado del buffer: siempre desde el principio
    AD1CON2bits.BUFM = 0;
    // El resultado queda siempre en ADC1BUF0.

    AD1CON2bits.ALTS = 0;   // Siempre se usa la entrada de MUXA
    // No se alternan entradas.

    AD1CON3bits.ADRC = 0;   // El reloj del CAD sale del reloj del sistema
    // Es un reloj preciso y conocido.


    AD1CON3bits.ADCS = 7; // MUY IMPORTANTE
    // El reloj del CAD (TAD) debe durar al menos 117.6 ns en 12 bits.
    // TAD = (ADCS + 1) * 16.69 ns, porque un ciclo de Fcy dura 16.69 ns.
    // Con ADCS = 7: TAD = 8 * 16.69 = 133.5 ns, que cumple.
    // Con ADCS = 6: TAD = 116.8 ns, que no cumple.

    // Los resultados se guardan en ADC1BUF0..ADC1BUFF. No se usa DMA.
    AD1CON4bits.ADDMAEN = 0;

    AD1CHS0bits.CH0SA = 0; // Entrada positiva del canal 0: AN0
    AD1CHS0bits.CH0NA = 0; // Entrada negativa del canal 0: VREFL (masa)

    IPC3bits.AD1IP = 4;    // Prioridad de la interrupción (de 1 a 7; 0 la desactiva)
    IFS0bits.AD1IF = 0;    // Borra el aviso antes de activar la interrupción
    IEC0bits.AD1IE = 1;    // Activa la interrupción del CAD

    AD1CON1bits.ADON = 1;  // Enciende el CAD
    // El CAD necesita 20 us para estabilizarse. No hay problema: la primera muestra
    // se toma 1/3 s después.
}

volatile char esperar_ADC = 1;       // 1 = el main espera la conversión; la ISR lo pone a 0
volatile int ADC_result = 0;         // Último valor leído (0 a 4095)

void __attribute__((interrupt(no_auto_psv))) _AD1Interrupt(void)
{
    esperar_ADC = 0;                 // Avisa al main: la conversión ha terminado
    ADC_result = ADC1BUF0;           // Guarda el resultado de la conversión
    IFS0bits.AD1IF = 0;              // Borra el aviso (si no se borra, la interrupción se repite)
}

/* ------------------------------------------------------------------------- */
/*  Timer4/5: avisa cada 1/3 s                                               */
/* ------------------------------------------------------------------------- */

volatile char esperar_timer = 1;     // 1 = el main espera al timer; la ISR lo pone a 0 cada 1/3 s

void init_t45(void)
{
    // 3 muestras por segundo = una cada 1/3 s = 59904000 / 3 = 19968000 ciclos.
    // Un timer de 16 bits no llega, así que se unen Timer4 y Timer5 en uno de 32 bits.
    // Con prescaler 1:256: 19968000 / 256 = 78000 cuentas, y PR = 78000 - 1 = 77999 = 0x000130AF.
    T4CONbits.TON = 0;               // Parado mientras se configura
    T4CONbits.TGATE = 0;
    T4CONbits.TCS = 0;               // Reloj interno (Fcy)
    T4CONbits.TCKPS = 0b11;          // Prescaler 1:256
    T4CONbits.T32 = 1;               // Timer4 y Timer5 forman un timer de 32 bits

    PR5 = 0x0001;                    // Parte alta del periodo
    PR4 = 0x30AF;                    // Parte baja del periodo

    // En 32 bits se usa la interrupción de Timer5
    IPC7bits.T5IP = 4;
    IFS1bits.T5IF = 0;
    IEC1bits.T5IE = 1;
    T4CONbits.TON = 1;               // Arranca el timer
}

void __attribute__((interrupt(no_auto_psv))) _T5Interrupt(void)
{
    esperar_timer = 0;               // Avisa al main: toca tomar una muestra
    IFS1bits.T5IF = 0;               // Borra el aviso
}

/* ------------------------------------------------------------------------- */
/*  main                                                                     */
/* ------------------------------------------------------------------------- */

int main(void) {
    init_clock_signal();
    init_uart1();
    init_adc();
    init_t45();

    while (1) {
        // Esperar aviso del timer (cada 1/3 s)
        while (esperar_timer);
        esperar_timer = 1;           // Se vuelve a poner a 1 para esperar el siguiente aviso

        // Comenzar muestreo (adquisición)
        AD1CON1bits.SAMP = 1;

        // Esperar adquisición: tiempo de carga del condensador al 99 %:
        // t = -RC*ln(0.01) = 2.699e-7 s; Tcy = 1/59.904 MHz = 1.669e-8 s
        // t / Tcy = 16.1 ciclos -> se redondea hacia arriba: 17
        __delay32(17);

        // Comenzar conversión
        AD1CON1bits.SAMP = 0;

        // Esperar interrupción del CAD (la conversión dura 14 TAD = 1.9 us)
        while (esperar_ADC);
        esperar_ADC = 1;             // Se vuelve a poner a 1 para la siguiente conversión

        // Enviar la muestra por la USART
        printf("%d\r\n", ADC_result);
    }
    return 0;
}