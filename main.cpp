#include "mbed.h"
#include "EthernetInterface.h"
#include "TCPSocket.h"
#include "string"

/* --------- build-time network selection ------------------------------ */
/*
#ifndef USE_DHCP                    // let your build system override if needed
    #define USE_DHCP     1         // 1 = DHCP (default), 0 = static
#endif

#if !USE_DHCP                       // only needed in static mode
    #define STATIC_IP      "192.168.68.50"
    #define STATIC_NETMASK "255.255.252.0"
    #define STATIC_GATEWAY "192.168.68.1"
#endif
*/

#define STATIC_IP      "192.168.68.60"
#define STATIC_NETMASK "255.255.252.0"
#define STATIC_GATEWAY "192.168.68.1"
/* --------------------------------------------------------------------- */

Thread thread_UpDownComm;

void ThreadFunc_HTTP_GET() 
{
    EthernetInterface net;
    while (1) 
    {
        printf("\n DOWNSIDE HTTP-SERVER STARTING...");                

        nsapi_error_t err = net.set_network(STATIC_IP, STATIC_NETMASK, STATIC_GATEWAY);
        if (err) 
        {
            printf("net.set_network() failed: %d\r\n", err);
        }
        printf("net.set_network() succeeded\r\n");

        /*
        err = net.connect();
        if (err) {
            printf("net.connect() failed: %d\r\n", err);
            break;
        }
        */

        /* HTTP Server ---------------------------------------------------------- */
        TCPSocket server;
        server.open(&net);
        server.bind(80);
        server.listen(5);
        printf("Listening on port 80...\r\n");

        while (1)
        {
            TCPSocket *client = server.accept();
            client->set_timeout(5000);

            std::string request;
            char buf[256];
            int total = 0;
        }
    }
}

int main()
{
    thread_UpDownComm.start(ThreadFunc_HTTP_GET);
}

