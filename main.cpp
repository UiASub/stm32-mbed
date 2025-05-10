#include "mbed.h"
#include "EthernetInterface.h"
#include "SocketAddress.h"
#include "http_request.h"          // from the mbed-http-client library

/* --------- build-time network selection ------------------------------ */
#ifndef USE_DHCP                    // let your build system override if needed
    #define USE_DHCP     1         // 1 = DHCP (default), 0 = static
#endif

#if !USE_DHCP                       // only needed in static mode
    #define STATIC_IP      "192.168.0.50"
    #define STATIC_NETMASK "255.255.255.0"
    #define STATIC_GATEWAY "192.168.0.1"
#endif
/* --------------------------------------------------------------------- */

int main()
{
    printf("\n=== NUCLEO-F767ZI HTTP-GET demo ===\r\n");

    EthernetInterface net;
    nsapi_error_t err = net.connect();
    if (err) {
        printf("net.connect() failed: %d\r\n", err);
        return 1;
    }

    /* MAC address comes back as a C string */
    printf("MAC  : %s\r\n", net.get_mac_address());

    /* ask the interface to fill a SocketAddress structure */
    SocketAddress ip;
    net.get_ip_address(&ip);
    printf("IP   : %s\r\n\n",
           ip.get_ip_address() ? ip.get_ip_address() : "None");

    /* HTTP GET ---------------------------------------------------------- */
    const char *url = "http://api.ipify.org";

    HttpRequest req(&net, HTTP_GET, url);
    HttpResponse *res = req.send();

    if (!res) {
        printf("HTTP request failed: %d\r\n", req.get_error());
    } else {
        printf("Status: %d (%s)\r\n",
               res->get_status_code(), res->get_status_message().c_str());
        printf("Body:\n%s\n", res->get_body_as_string().c_str());
    }

    while (1) {
        ThisThread::sleep_for(10);
    }

    net.disconnect();
    printf("\nDone – interface shut down.\r\n");
}