#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

#include "FreeRTOS.h"
#ifndef portTICK_RATE_MS
    #define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

// Core FreeRTOS and architecture configurations
#define NO_SYS                          0
#define TCPIP_THREAD_PRIO               3
#define TCPIP_THREAD_STACKSIZE          1024
#define DEFAULT_THREAD_STACKSIZE        1024
#define DEFAULT_RAW_RECVMBOX_SIZE       8
#define TCPIP_MBOX_SIZE                 8
#define LWIP_TIMEVAL_PRIVATE            0

// Memory and Buffer Tuning
#define MEM_LIBC_MALLOC                 0
#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        4000
#define MEMP_NUM_TCP_SEG                32
#define MEMP_NUM_ARP_QUEUE              10
#define PBUF_POOL_SIZE                  24
#define PBUF_POOL_BUFSIZE               1514

// Feature Enables
#define LWIP_ARP                        1
#define LWIP_ETHERNET                   1
#define LWIP_ICMP                       1
#define LWIP_RAW                        1
#define LWIP_TCP                        1
#define LWIP_UDP                        1
#define LWIP_DHCP                       1
#define LWIP_DNS                        1
#define LWIP_NETCONN                    1
#define LWIP_SOCKET                     1
#define LWIP_CHKSUM_ALGORITHM           3
#define LWIP_STATS                      1

// Interface and Callback settings
#define LWIP_NETIF_LINK_CALLBACK        1
#define LWIP_NETIF_STATUS_CALLBACK      1
#define LWIP_NETIF_EXT_STATUS_CALLBACK  1

#endif /* _LWIPOPTS_H */
