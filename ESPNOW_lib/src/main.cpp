/*
Etienne Arlaud
*/

#include <stdint.h>
#include <stdio.h>

#include <assert.h>
#include <unistd.h>
#include <sys/time.h>

#include <thread>

#include "ESPNOW_manager.h"
#include "ESPNOW_types.h"

using namespace std;

static uint8_t my_mac[6] = {0x48, 0x89, 0xE7, 0xFA, 0x60, 0x7C};
static uint8_t dest_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};	// broadcast addr
static uint8_t ESP_mac[6] = {0xC8,0x2B,0x96,0xB4,0xE6,0xCC};

ESPNOW_manager *handler;

uint8_t payload[127];

/* Funcion para imprimir el contenido del paquete */
void print_packet(uint8_t *data, int len)
{
    printf("----------------------------new packet-----------------------------------\n");
    int i;
    for (i = 0; i < len; i++)
    {
        if (i % 16 == 0)
            printf("\n");
        printf("0x%02x, ", data[i]);
    }
    printf("\n\n");
}

/* Funcion de callback al recibir datos */
void callback(uint8_t src_mac[6], uint8_t *data, int len) {
	handler->mypacket.wlan.actionframe.content.length = 127 + 5;
	memcpy(handler->mypacket.wlan.actionframe.content.payload, data, 6);

	printf("Package Received\n");
	print_packet(data, len);
	//handler->send();
}

int main(int argc, char **argv) {
	assert(argc > 1);

	nice(-20);	// setea la prioridad del proceso -> rango es de [-20, 19], con -20 la mas alta prioridad

	handler = new ESPNOW_manager(argv[1], DATARATE_24Mbps, CHANNEL_freq_1, my_mac, dest_mac, false);

	handler->set_filter(ESP_mac, dest_mac);

	handler->set_recv_callback(&callback);

	handler->start();

	while(1) {
		std::this_thread::yield();
	}

	handler->end();
}
