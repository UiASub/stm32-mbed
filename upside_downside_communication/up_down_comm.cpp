/**
 * @file up_down_comm.hpp
 * @author Simen Skretteberg Hanisch
 */

#include "up_down_comm.hpp"
#include "../json/include/nlohmann/json.hpp"
#include <cstring>

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
/* --------------------------------------------------------------------- */

#define STATIC_IP      "192.168.68.60"
#define STATIC_NETMASK "255.255.252.0"
#define STATIC_GATEWAY "192.168.68.1"

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

        mailTx_t tempRx;
        tempRx.velocity = 0.0f;
        tempRx.acceleration = 0.0f;

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
            std::string bodyRx;
            if (request.rfind("POST", 0) == 0)
            {
                size_t body_pos = request.find("\r\n\r\n");
                if (body_pos != std::string::npos)
                {
                    bodyRx = request.substr(body_pos + 4, content_length);
                    printf("\nPOST-body (%d bytes): %s\n", content_length, bodyRx.c_str());
                }
            }

            /* === Process data to send === */
            // Get data from Tx Queue
            mailTx_t *txData = mailTx.try_get();
            if (txData != nullptr)
            {
                // Remember last sent acceleration/velocity data
                tempRx.acceleration = txData->acceleration;
                tempRx.velocity = txData->velocity;
            }
            // Free Tx Queue
            mailTx.free(txData);

            /* === Create HTTP-response to send with data === */
            const char *response_fmt = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Connection: close\r\n"
                "\r\n"
                "{\r\n"
                "   \"Displacement\": {\r\n"
                "       \"velocity\": %f\r\n"
                "       \"acceleration\": %f\r\n"
                "   }\r\n"
                "}\r\n";
            char response[256];
            int len = sprintf(response, response_fmt, tempRx.velocity, tempRx.acceleration);
            client->send(response, len);

            client->close();

            /* === Process received data === */
            cJSON *parsedRxData = cJSON_ParseWithLength((const char*)(bodyRx.c_str()), strlen(bodyRx.c_str()));
            if (parsedRxData == NULL)
            {
                const char *error_ptr = cJSON_GetErrorPtr();
                if (error_ptr != NULL)
                {
                    // printf("cJSON error: %i", error_ptr);
                }
                cJSON_Delete(parsedRxData);
                break;
            }

            // Put processed data on Rx Queue 
            mailRx_t *rxData = mailRx.try_alloc();
            if (rxData != nullptr)
            {
                /* Begin Target Velocity */
                // x
                cJSON *x = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(parsedRxData, "Target"), "velocity"), "x");
                rxData->x = (x != NULL) ? (float)x->valuedouble : 0.0f;
                // y
                cJSON *y = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(parsedRxData, "Target"), "velocity"), "y");
                rxData->y = (y != NULL) ? (float)y->valuedouble : 0.0f;
                // z
                cJSON *z = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(parsedRxData, "Target"), "velocity"), "z");
                rxData->z = (z != NULL) ? (float)z->valuedouble : 0.0f;
                // yaw
                cJSON *yaw = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(parsedRxData, "Target"), "velocity"), "yaw");
                rxData->yaw = (yaw != NULL) ? (float)yaw->valuedouble : 0.0f;
                // pitch
                cJSON *pitch = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(parsedRxData, "Target"), "velocity"), "pitch");
                rxData->pitch = (pitch != NULL) ? (float)pitch->valuedouble : 0.0f;
                // roll
                cJSON *roll = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(parsedRxData, "Target"), "velocity"), "roll");
                rxData->roll = (roll != NULL) ? (float)roll->valuedouble : 0.0f;
                /* End Target Velocity */

                /* Begin Lights */
                // led1
                cJSON *light1 = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(parsedRxData, "Target"), "lights"), "light1");
                rxData->led1_dimm = (light1 != NULL) ? (uint8_t)light1->valueint : 0;
                // led2
                cJSON *light2 = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(parsedRxData, "Target"), "lights"), "light2");
                rxData->led2_dimm = (light2 != NULL) ? (uint8_t)light2->valueint : 0;
                // led3
                cJSON *light3 = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(parsedRxData, "Target"), "lights"), "light3");
                rxData->led3_dimm = (light3 != NULL) ? (uint8_t)light3->valueint : 0;
                // led4
                cJSON *light4 = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(parsedRxData, "Target"), "lights"), "light4");
                rxData->led4_dimm = (light4 != NULL) ? (uint8_t)light4->valueint : 0;
                /* End Lights */

                /* Begin Control Inputs */
                // depth
                cJSON *depth = cJSON_GetObjectItem(cJSON_GetObjectItem(parsedRxData, "Target"), "depth");
                rxData->depth = (depth != NULL) ? (uint16_t)depth->valueint : 0;
                // emergency ascent
                cJSON *ascent = cJSON_GetObjectItem(cJSON_GetObjectItem(parsedRxData, "Target"), "emergency_ascent");
                rxData->emergency_ascent = (ascent != NULL) ? (size_t)ascent->valueint : 0;
                /* End Control Inputs */
            }
            cJSON_Delete(parsedRxData);
        }
        net.disconnect();
        osThreadExit();
    }
}
