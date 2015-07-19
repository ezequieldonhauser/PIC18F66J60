#include "TCPIPConfig.h"
#include "TCPIP Stack/TCPIP.h"
#include "ServerSocket.h"

void longtoa(char * buf, unsigned long val, int base)
{
   unsigned   long   v;
   char      c;

   v = val;
   do {
      v /= base;
      buf++;
   } while(v != 0);
   *buf-- = 0;
   do {
      c = val % base;
      val /= base;
      if(c >= 10)
         c += 'A'-'0'-10;
      c += '0';
      *buf-- = c;
   } while(val != 0);
}

void eztoa(char * buf, int val, int base)
{
   if(val < 0) {
      *buf++ = '-';
      val = -val;
   }
   rrtoa(buf, val, base);
}
void rrtoa(char * buf, unsigned val, int base)
{
   unsigned   v;
   char      c;

   v = val;
   do {
      v /= base;
      buf++;
   } while(v != 0);
   *buf-- = 0;
   do {
      c = val % base;
      val /= base;
      if(c >= 10)
         c += 'A'-'0'-10;
      c += '0';
      *buf-- = c;
   } while(val != 0);
}

//maquina de estado servidor socket TCP
void ServerSocketTCP(void)
{
    BYTE AppBuffer[20];
    WORD bytes_fifo_rx;
    static TCP_SOCKET	MySocket;

    static enum _TCPServerState
    {
        SM_HOME = 0,
        SM_LISTENING,
                
    }maquina_estado_servidor = SM_HOME;

    //maquina de estado
    switch(maquina_estado_servidor)
    {
        //----------------------------------------------------------------------
        case SM_HOME:

            // Allocate a socket for this server to listen and accept connections on
            MySocket = TCPOpen(0, TCP_OPEN_SERVER, SERVER_PORT, TCP_PURPOSE_GENERIC_TCP_SERVER);
            //retorna senao consegir abrir um socket
            if(MySocket == INVALID_SOCKET)
            return;
            //se abriu fica escutando a porta
            maquina_estado_servidor = SM_LISTENING;

        break;
        //----------------------------------------------------------------------
        case SM_LISTENING:

            //verifica se tem algum socket aberto
            if(!TCPIsConnected(MySocket))
            return;

            //veficar quantos bytes tem no buffer aguardando
            bytes_fifo_rx=TCPIsGetReady(MySocket);

            if(bytes_fifo_rx>1)
            {
                // Pega os dados da FIFO RX
                TCPGetArray(MySocket, AppBuffer, sizeof(AppBuffer));

                if(strcmppgm2ram((char *)AppBuffer,(ROM char *)"OI") == 0)
                {
                    TCPPut(MySocket, 'S');
                    TCPPut(MySocket, 'I');
                    TCPPut(MySocket, 'M');
                    TCPPut(MySocket, '\r');
                    TCPPut(MySocket, '\n');
                }
                else
                {
                    TCPPut(MySocket, 'N');
                    TCPPut(MySocket, 'A');
                    TCPPut(MySocket, 'O');
                    TCPPut(MySocket, '\r');
                    TCPPut(MySocket, '\n');
                }
                TCPFlush(MySocket);
            }

        break;
        //----------------------------------------------------------------------
    }
}