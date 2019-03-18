
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_arp.h>
#include <arpa/inet.h>
#include <assert.h>

#include "ESPNOW_handler.h"

#define MAC_2_MSBytes(MAC)  (MAC[0] << 8) && MAC[1]
#define MAC_4_LSBytes(MAC)  (((((MAC[2] << 8) && MAC[3]) << 8) && MAC[4]) << 8) && MAC[5]

void ESPNOW_handler::set_dest_mac(uint8_t dest_mac[8]) {
	memcpy(this->dest_mac, dest_mac, sizeof(uint8_t)*6);
}

void ESPNOW_handler::set_interface(char* interface) {
	this->interface = (char*) malloc(strlen(interface)*sizeof(char));	
	strcpy(this->interface, interface);
}

void ESPNOW_handler::set_filter(uint8_t *src_mac, uint8_t *dst_mac) {
	//sudo tcpdump -i wlp5s0 'type 0 subtype 0xd0 and wlan[24:4]=0x7f18fe34 and wlan[32]=221 and wlan[33:4]&0xffffff = 0x18fe34 and wlan[37]=0x4 and wlan dst 11:22:33:44:55:66 and wlan src 77:88:99:aa:bb:cc' -dd
	this->bpf.len = 53;
	
	uint32_t MSB_dst = MAC_2_MSBytes(dst_mac);
	uint32_t LSB_dst = MAC_4_LSBytes(dst_mac);

	uint32_t MSB_src = MAC_2_MSBytes(dst_mac);
	uint32_t LSB_src = MAC_4_LSBytes(dst_mac);
	
	struct sock_filter temp_code[this->bpf.len] = {
			{ 0x30, 0, 0, 0x00000003 },
			{ 0x64, 0, 0, 0x00000008 },
			{ 0x7, 0, 0, 0x00000000 },
			{ 0x30, 0, 0, 0x00000002 },
			{ 0x4c, 0, 0, 0x00000000 },
			{ 0x2, 0, 0, 0x00000000 },
			{ 0x7, 0, 0, 0x00000000 },
			{ 0x50, 0, 0, 0x00000000 },
			{ 0x54, 0, 0, 0x000000fc },
			{ 0x15, 0, 42, 0x000000d0 },
			{ 0x40, 0, 0, 0x00000018 },
			{ 0x15, 0, 40, 0x7f18fe34 },
			{ 0x50, 0, 0, 0x00000020 },
			{ 0x15, 0, 38, 0x000000dd },
			{ 0x40, 0, 0, 0x00000021 },
			{ 0x54, 0, 0, 0x00ffffff },
			{ 0x15, 0, 35, 0x0018fe34 },
			{ 0x50, 0, 0, 0x00000025 },
			{ 0x15, 0, 33, 0x00000004 },
			{ 0x50, 0, 0, 0x00000000 },
			{ 0x45, 31, 0, 0x00000004 },
			{ 0x45, 0, 21, 0x00000008 },
			{ 0x50, 0, 0, 0x00000001 },
			{ 0x45, 0, 4, 0x00000001 },
			{ 0x40, 0, 0, 0x00000012 },
			{ 0x15, 0, 26, LSB_dst },
			{ 0x48, 0, 0, 0x00000010 },
			{ 0x15, 4, 24, MSB_dst },
			{ 0x40, 0, 0, 0x00000006 },
			{ 0x15, 0, 22, LSB_dst },
			{ 0x48, 0, 0, 0x00000004 },
			{ 0x15, 0, 20, MSB_dst },
			{ 0x50, 0, 0, 0x00000001 },
			{ 0x45, 0, 13, 0x00000002 },
			{ 0x45, 0, 4, 0x00000001 },
			{ 0x40, 0, 0, 0x0000001a },
			{ 0x15, 0, 15, LSB_src },
			{ 0x48, 0, 0, 0x00000018 },
			{ 0x15, 12, 13, MSB_src },
			{ 0x40, 0, 0, 0x00000012 },
			{ 0x15, 0, 11, LSB_src },
			{ 0x48, 0, 0, 0x00000010 },
			{ 0x15, 8, 9, MSB_src },
			{ 0x40, 0, 0, 0x00000006 },
			{ 0x15, 0, 7, LSB_dst },
			{ 0x48, 0, 0, 0x00000004 },
			{ 0x15, 0, 5, MSB_dst },
			{ 0x40, 0, 0, 0x0000000c },
			{ 0x15, 0, 3, LSB_src },
			{ 0x48, 0, 0, 0x0000000a },
			{ 0x15, 0, 1, MSB_src },
			{ 0x6, 0, 0, 0x00040000 },
			{ 0x6, 0, 0, 0x00000000 }
						};
	
	this->bpf.filter = (sock_filter*) malloc(sizeof(sock_filter)*this->bpf.len);
	memcpy(this->bpf.filter, temp_code, sizeof(struct sock_filter) * this->bpf.len);
}


void ESPNOW_handler::start() {
	struct sockaddr_ll s_dest_addr;
    struct ifreq ifr;
	
    int fd, 			//file descriptor
		ioctl_errno,	//ioctl errno
		bind_errno,		//bind errno
		filter_errno,	//attach filter errno
		priority_errno;	//Set priority errno
	
	bzero(&s_dest_addr, sizeof(s_dest_addr));
    bzero(&ifr, sizeof(ifr));
	
	
    fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    assert(fd != -1); 			//abort if error

    strncpy((char *)ifr.ifr_name, this->interface, IFNAMSIZ); //interface

    ioctl_errno = ioctl(fd, SIOCGIFINDEX, &ifr);
    assert(ioctl_errno >= 0);	//abort if error

    s_dest_addr.sll_family = PF_PACKET;
    //we don't use a protocol above ethernet layer, just use anything here
    s_dest_addr.sll_protocol = htons(ETH_P_ALL);
    s_dest_addr.sll_ifindex = ifr.ifr_ifindex;
    s_dest_addr.sll_hatype = ARPHRD_ETHER;
    s_dest_addr.sll_pkttype = PACKET_OTHERHOST; //PACKET_OUTGOING
    s_dest_addr.sll_halen = ETH_ALEN;
	memcpy(&(s_dest_addr.sll_addr), this->dest_mac, sizeof(this->dest_mac)*6); //MAC

    bind_errno = bind(fd, (struct sockaddr *)&s_dest_addr, sizeof(s_dest_addr));
    assert(bind_errno >= 0);	//abort if error
	
	if(bpf.len > 0) {
		filter_errno = setsockopt(fd, SOL_SOCKET, SO_ATTACH_FILTER, &(this->bpf), sizeof(bpf));
		assert(filter_errno >= 0);
	}

	priority_errno = setsockopt(fd, SOL_SOCKET, SO_PRIORITY, &(this->socket_priority), sizeof(this->socket_priority));
	assert(priority_errno ==0);
	

    this->sock_fd = fd;
}

int ESPNOW_handler::send(ESPNOW_packet p) {
	int len = p.toBytes(this->raw_bytes, LEN_MAX_RAW_BYTES);
	return sendto(this->sock_fd, this->raw_bytes, len, 0, NULL, 0);
}