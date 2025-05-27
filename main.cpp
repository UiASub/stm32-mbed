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

typedef struct {
    float acceleration;
    float velocity;
} mailTx_t;

typedef struct {
    // Velocity
    float x;
    float y;
    float z;
    float yaw;
    float pitch;
    float roll;
    // Lights
    uint8_t led1_dimm;
    uint8_t led2_dimm;
    uint8_t led3_dimm;
    uint8_t led4_dimm;
    // Control
    uint16_t depth;
    size_t emergency_ascent;
} mailRx_t;

Mail<mailTx_t, 8> mailTx;
Mail<mailRx_t, 8> mailRx;

Thread thread_UpDownComm;

void ThreadFunc_HTTP_GET() 
{
    EthernetInterface net;
    while (1) 
    {
        printf("\nDOWNSIDE HTTP-SERVER STARTING...\r\n");                

        nsapi_error_t err = net.set_network(STATIC_IP, STATIC_NETMASK, STATIC_GATEWAY);
        if (err) 
        {
            printf("net.set_network() failed: %d\r\n", err);
        }
        printf("net.set_network() succeeded\r\n");

        err = net.connect();
        if (err)
        {
            printf("net.connect() failed: %d\r\n", err);
        }
        printf("net.connect() succeeded\r\n");


        /* HTTP Server ---------------------------------------------------------- */
        TCPSocket server;
        server.open(&net);
        server.bind(80);
        server.listen(5);
        printf("Listening on port 80...\r\n");

        mailTx_t temp_Rx;

        while (1)
        {
            TCPSocket *client = server.accept();
            client->set_timeout(5000);

            std::string request;
            char buf[256];
            int total = 0;
            int content_length = 0;
            size_t headers_read = 0;

            while (1)
            {
                int r = client->recv(buf, sizeof(buf));
                if (r <= 0) break;
                request.append(buf, r);
                total += r;

                if (!headers_read)
                {
                    size_t pos = request.find("\r\n\r\n");
                    if (pos != std::string::npos)
                    {
                        headers_read = 1;

                        size_t cl_pos = request.find("Content-Length:");
                        if (cl_pos != std::string::npos)
                        {
                            cl_pos += strlen("Content-Length:");
                            content_length = atoi(request.substr(cl_pos, request.find("\r\n", cl_pos) - cl_pos).c_str());
                        }
                    }
                }
                if (headers_read)
                {
                    size_t header_end = request.find("\r\n\r\n") + 4;
                    if ((int)(request.size() - header_end) >= content_length) break;
                }
            }
            printf("\n=== Receiving HTTP request ===\n");
            //printf("%s\n", request.c_str());

            if (request.rfind("POST", 0) == 0)
            {
                size_t body_pos = request.find("\r\n\r\n");
                if (body_pos != std::string::npos)
                {
                    std::string body = request.substr(body_pos + 4, content_length);
                    printf("\nPOST-body (%d bytes): %s\n", content_length, body.c_str());
                }
            }

            /* === Process data to send === */
            // Get data from Tx Queue
            mailTx_t *Tx_Data = mailTx.try_get();
            // Remember last sent acceleration/velocity data
            temp_Rx.acceleration = Tx_Data->acceleration;
            temp_Rx.velocity = Tx_Data->velocity;
            // Free Tx Queue
            mailTx.free(Tx_Data);

            /* === Create HTTP-response to send with data === */

            const char *response_fmt = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Connection: close\r\n"
                "\r\n"
                "Received %d byte in POST\n";
            char response[128];
            int len = sprintf(response, response_fmt, content_length);
            client->send(response, len);  

            client->close();

            /* === Process received data === */
            
            // Put processed data on Rx Queue 
            mailRx_t *Rx_Data = mailRx.try_alloc();


        }
        net.disconnect();
        osThreadExit();
    }
}

int main()
{
    thread_UpDownComm.start(ThreadFunc_HTTP_GET);
}