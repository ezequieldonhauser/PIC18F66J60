#include <p18f66j60.h>

#pragma config WDT = OFF        // Watchdog Timer Enable bit (WDT disabled (control is placed on SWDTEN bit))
#pragma config STVR = ON        // Stack Overflow/Underflow Reset Enable bit (Reset on stack overflow/underflow enabled)
#pragma config XINST = OFF      // Extended Instruction Set Enable bit (Instruction set extension and Indexed Addressing mode disabled (Legacy mode))
#pragma config CP0 = OFF        // Code Protection bit (Program memory is not code-protected)
#pragma config FOSC = HSPLL     // Oscillator Selection bits (HS oscillator, PLL enabled and under software control)
#pragma config FOSC2 = ON       // Default/Reset System Clock Select bit (Clock selected by FOSC1:FOSC0 as system clock is enabled when OSCCON<1:0> = 00)
#pragma config FCMEN = ON       // Fail-Safe Clock Monitor Enable (Fail-Safe Clock Monitor enabled)
#pragma config IESO = ON        // Two-Speed Start-up (Internal/External Oscillator Switchover) Control bit (Two-Speed Start-up enabled)
#pragma config WDTPS = 32768    // Watchdog Timer Postscaler Select bits (1:32768)
#pragma config ETHLED = ON      // Ethernet LED Enable bit (RA0/RA1 are multiplexed with LEDA/LEDB when Ethernet module is enabled and function as I/O when Ethernet is disabled)

#include <stdlib.h>
#include "TCPIP Stack/TCPIP.h"
#include "MainDemo.h"
#include "LCDBlocking.h"
#include "TCPIPConfig.h"
#include "ClientSocket.h"
#include "ServerSocket.h"

//------------------------------interrupção-------------------------------------
#pragma interruptlow LowISR
void LowISR(void)
{
    TickUpdate();
}
#pragma interruptlow HighISR
void HighISR(void)
{
    #if defined(STACK_USE_UART2TCP_BRIDGE)
                UART2TCPBridgeISR();
    #endif
}
#pragma code lowVector=0x18
void LowVector(void){_asm goto LowISR _endasm}
#pragma code highVector=0x8
void HighVector(void){_asm goto HighISR _endasm}
#pragma code // Return to default code section
//------------------------------------------------------------------------------

static void ProcessIO(void);
unsigned char AN0String[10];

//função principal
void main(void)
{
    static TICK t = 0;

    //inicializa todas as configuraçoes de hardware
    InitializeBoard();

    #if defined(USE_LCD)
    //inicializa configs do LCD se acaso estiver habilitado
    LCDInit();
    DelayMs(100);
    strcpypgm2ram((char*)LCDText, "APP TCPIP");
    LCDUpdate();
    #endif

    //inicializa um tick de tempo usado para TICK,SPI,UAT
    TickInit();

    //inicializa MPSF para upload de paginas web se acaso estiver habilitado
    #if defined(STACK_USE_MPFS) || defined(STACK_USE_MPFS2)
    MPFSInit();
    #endif

    //inicializa variaveis da aplicação AppConfig (IP, MASCARA, GATWAY, ETC)
    InitAppConfig();

    //inicializa a layer da pilha TCPIP (MAC, ARP, TCP, UDP)
    //e tambem as aplicaçoes habilitadas (HTTP, SNMP, SOCKET, ETC)
    StackInit();

    //inicializa UART 2 TCP Bridge
    #if defined(STACK_USE_UART2TCP_BRIDGE)
    UART2TCPBridgeInit();
    #endif

    //laço principal (nunca use delays, apenas maquinas de estado)
    //todos os processos devem estar executando paralelamente
    while(1)
    {
        //pisca o led para informar a pilha rodando
        if(TickGet() - t >= TICK_SECOND/2ul)
        {
            t = TickGet();
            LED0_IO ^= 1;
        }

        //processa coisas relacionadas ao hardware, leitura de pinos,etc.
        ProcessIO();

        //chama tarefas da pilha TCPIP
        StackTask();

        //chama tarefas das aplicaçoes habilitadas
        StackApplications();

        //exemplo de aplicação Cliente Socket
        #if defined(STACK_USE_TCP_CLIENT)
        ClientSocketTCP();
        #endif

        //exemplo de aplicação Servidor Socket
        #if defined(STACK_USE_TCP_SERVER)
        ServerSocketTCP();
        #endif
    }
}

// Processes A/D data from the potentiometer
static void ProcessIO(void)
{
    BYTE temp;
    // AN0 should already be set up as an analog input
    ADCON0bits.GO = 1;

    // Wait until A/D conversion is done
    while(ADCON0bits.GO);

    temp = ADCON2;
    ADCON2 |= 0x7;    // Select Frc mode by setting ADCS0/ADCS1/ADCS2
    ADCON2 = temp;

    // Convert 10-bit value into ASCII string
    uitoa(*((WORD*)(&ADRESL)), AN0String);
}

//inicializa todas as configuraçoes de hardware
static void InitializeBoard(void)
{	
    // LEDs
    LED0_TRIS = 0;
    LED1_TRIS = 0;
    LED0_IO   = 1;
    LED1_IO   = 1;

    //botoes
    BUTTON0_TRIS = 1;
    BUTTON1_TRIS = 1;

    // Enable 4x/5x/96MHz PLL on PIC18F87J10, PIC18F97J60, PIC18F87J50, etc.
    OSCTUNE = 0x40;

    // Set up analog features of PORTA
    ADCON0 = 0b00001001;	//ADON, Channel 2 (AN2)
    ADCON1 = 0b00001100;	//VSS0 VDD0, AN0,AN1,AN2 is analog
    ADCON2 = 0xBE;		//Right justify, 20TAD ACQ time, Fosc/64 (~21.0kHz)

    // Disable internal PORTB pull-ups
    INTCON2bits.RBPU = 1;

    // Configure USART
    TXSTA = 0x20;
    RCSTA = 0x90;

    // See if we can use the high baud rate setting
    #if ((GetPeripheralClock()+2*BAUD_RATE)/BAUD_RATE/4 - 1) <= 255
    SPBRG = (GetPeripheralClock()+2*BAUD_RATE)/BAUD_RATE/4 - 1;
    TXSTAbits.BRGH = 1;
    #else	// Use the low baud rate setting
    SPBRG = (GetPeripheralClock()+8*BAUD_RATE)/BAUD_RATE/16 - 1;
    #endif

        PIE1bits.RCIE = 1;
    // Enable Interrupts
    RCONbits.IPEN = 1;		// Enable interrupt priorities
    INTCONbits.GIEH = 1;
    INTCONbits.GIEL = 1;

    // Do a calibration A/D conversion
    ADCON0bits.ADCAL = 1;
    ADCON0bits.GO = 1;
    while(ADCON0bits.GO);
    ADCON0bits.ADCAL = 0;

    #if defined(SPIRAM_CS_TRIS)
            SPIRAMInit();
    #endif
    #if defined(EEPROM_CS_TRIS)
            XEEInit();
    #endif
    #if defined(SPIFLASH_CS_TRIS)
            SPIFlashInit();
    #endif
}

//inicialização geral das variaveis da aplicação
static ROM BYTE SerializedMACAddress[6]=
{ MY_DEFAULT_MAC_BYTE1, MY_DEFAULT_MAC_BYTE2, MY_DEFAULT_MAC_BYTE3,
  MY_DEFAULT_MAC_BYTE4, MY_DEFAULT_MAC_BYTE5, MY_DEFAULT_MAC_BYTE6};
static void InitAppConfig(void)
{
    AppConfig.Flags.bIsDHCPEnabled = TRUE;
    AppConfig.Flags.bInConfigMode = TRUE;
    memcpypgm2ram((void*)&AppConfig.MyMACAddr, (ROM void*)SerializedMACAddress, sizeof(AppConfig.MyMACAddr));
    memcpypgm2ram(AppConfig.NetBIOSName, (ROM void*)MY_DEFAULT_HOST_NAME, 16);
    FormatNetBIOSName(AppConfig.NetBIOSName);
    AppConfig.MyIPAddr.Val = MY_DEFAULT_IP_ADDR_BYTE1 | MY_DEFAULT_IP_ADDR_BYTE2<<8ul | MY_DEFAULT_IP_ADDR_BYTE3<<16ul | MY_DEFAULT_IP_ADDR_BYTE4<<24ul;
    AppConfig.DefaultIPAddr.Val = AppConfig.MyIPAddr.Val;
    AppConfig.MyMask.Val = MY_DEFAULT_MASK_BYTE1 | MY_DEFAULT_MASK_BYTE2<<8ul | MY_DEFAULT_MASK_BYTE3<<16ul | MY_DEFAULT_MASK_BYTE4<<24ul;
    AppConfig.DefaultMask.Val = AppConfig.MyMask.Val;
    AppConfig.MyGateway.Val = MY_DEFAULT_GATE_BYTE1 | MY_DEFAULT_GATE_BYTE2<<8ul | MY_DEFAULT_GATE_BYTE3<<16ul | MY_DEFAULT_GATE_BYTE4<<24ul;
    AppConfig.PrimaryDNSServer.Val = MY_DEFAULT_PRIMARY_DNS_BYTE1 | MY_DEFAULT_PRIMARY_DNS_BYTE2<<8ul  | MY_DEFAULT_PRIMARY_DNS_BYTE3<<16ul  | MY_DEFAULT_PRIMARY_DNS_BYTE4<<24ul;
    AppConfig.SecondaryDNSServer.Val = MY_DEFAULT_SECONDARY_DNS_BYTE1 | MY_DEFAULT_SECONDARY_DNS_BYTE2<<8ul  | MY_DEFAULT_SECONDARY_DNS_BYTE3<<16ul  | MY_DEFAULT_SECONDARY_DNS_BYTE4<<24ul;

    #if defined(EEPROM_CS_TRIS)
    {
        BYTE c;

        // When a record is saved, first byte is written as 0x60 to indicate
        // that a valid record was saved.  Note that older stack versions
        // used 0x57.  This change has been made to so old EEPROM contents
        // will get overwritten.  The AppConfig() structure has been changed,
        // resulting in parameter misalignment if still using old EEPROM
        // contents.
        XEEReadArray(0x0000, &c, 1);
        if(c == 0x42u)
        {
            XEEReadArray(0x0001, (BYTE*)&AppConfig, sizeof(AppConfig));
        }
    else
        SaveAppConfig();
    }
    #elif defined(SPIFLASH_CS_TRIS)
    {
        BYTE c;

        SPIFlashReadArray(0x0000, &c, 1);
        if(c == 0x42u)
        {
            SPIFlashReadArray(0x0001, (BYTE*)&AppConfig, sizeof(AppConfig));
        }
        else
            SaveAppConfig();
    }
    #endif
}


