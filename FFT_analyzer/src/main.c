#include <stdio.h>
#include "xparameters.h"
#include "xil_printf.h"
#include "xaxidma.h"
#include "xgpio.h"
#include "xil_cache.h"
#include "sleep.h"
#include "lwip/inet.h"
#include "lwip/timeouts.h"
#include "lwip/err.h"
#include "lwip/udp.h"
#include "lwip/init.h"
#include "lwipopts.h"
#include "netif/xadapter.h"
#include "platform.h"
#include "platform_config.h"

// ================= NETWORK CONFIGURATION =================
#define BOARD_IP_ADDR "192.168.1.10"
#define NETMASK_ADDR  "255.255.255.0"
#define GATEWAY_ADDR  "192.168.1.1"

// Receiver settings (Your PC)
#define PC_IP_ADDR    "192.168.1.100" // PC IP HERE
#define PC_UDP_PORT   12345           // Port that the PC will listen on

// ================= HARDWARE CONFIGURATION =================
#define FFT_LENGTH 1024
// Spectrum buffer (cache aligned)
u32 fft_buffer[FFT_LENGTH] __attribute__((aligned(32)));

XAxiDma AxiDma;
XGpio AdcGpio;

struct netif echo_netif;
struct udp_pcb *pcb;
ip_addr_t pc_ipaddr;

//Hardware initialization functions
int init_hardware() {
    int Status;
    xil_printf("Initializing GPIO...\r\n");
    Status = XGpio_Initialize(&AdcGpio, XPAR_XGPIO_0_BASEADDR);
    if (Status != XST_SUCCESS) return XST_FAILURE;

    xil_printf("Initializing DMA...\r\n");
    XAxiDma_Config *CfgPtr = XAxiDma_LookupConfig(XPAR_XAXIDMA_0_BASEADDR);
    if (!CfgPtr) return XST_FAILURE;
    Status = XAxiDma_CfgInitialize(&AxiDma, CfgPtr);
    if (Status != XST_SUCCESS) return XST_FAILURE;

    XAxiDma_IntrDisable(&AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);
    return XST_SUCCESS;
}

void start_adc() {
    xil_printf("Starting ADC and FFT...\r\n");
    XGpio_DiscreteWrite(&AdcGpio, 1, 10); // Clock divider
    XGpio_DiscreteWrite(&AdcGpio, 2, 1);  // Enable = 1
}

// Network setup function
int setup_network() {
    ip_addr_t ipaddr, netmask, gw;
    int status;
    
    // Set a local MAC address for our board
    unsigned char mac_ethernet_address[] = { 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };

    init_platform();

    xil_printf("Configuring network...\r\n");

    inet_aton(BOARD_IP_ADDR, &ipaddr);
    inet_aton(NETMASK_ADDR, &netmask);
    inet_aton(GATEWAY_ADDR, &gw);
    inet_aton(PC_IP_ADDR, &pc_ipaddr);

    lwip_init();

    if (!xemac_add(&echo_netif, &ipaddr, &netmask, &gw, mac_ethernet_address, PLATFORM_EMAC_BASEADDR)) {
        xil_printf("Error adding network interface\r\n");
        return -1;
    }
    
    netif_set_default(&echo_netif);
    netif_set_up(&echo_netif);

    pcb = udp_new();
    if (!pcb) {
        xil_printf("Error creating UDP PCB\r\n");
        return -1;
    }

    status = udp_connect(pcb, &pc_ipaddr, PC_UDP_PORT);
    if (status != ERR_OK) {
         xil_printf("Error connecting UDP\r\n");
         return -1;
    }

    xil_printf("Network ready. Zynq IP: %s, Sending to: %s:%d\r\n", 
               BOARD_IP_ADDR, PC_IP_ADDR, PC_UDP_PORT);
    return 0;
}

// send buffer via UDP
void send_spectrum_udp(u32 *data_ptr, u16 length_words) {
    struct pbuf *p;
    err_t err;

    u16 length_bytes = length_words * sizeof(u32);

    // Allocate packet buffer
    p = pbuf_alloc(PBUF_TRANSPORT, length_bytes, PBUF_RAM);
    if (!p) {
        xil_printf("Error allocating pbuf! (Sending too fast?)\r\n");
        return;
    }

    // Copy spectrum data to the packet buffer
    err = pbuf_take(p, data_ptr, length_bytes);
    if (err != ERR_OK) {
        xil_printf("Error copying to pbuf\r\n");
        pbuf_free(p);
        return;
    }

    // Send UDP packet
    err = udp_send(pcb, p);
    if (err != ERR_OK) {
        xil_printf("Error udp_send: %d\r\n", err);
    }

    pbuf_free(p);
}


int main() {
    int Status;

    xil_printf("\r\n--- Starting Spectrometer (Ethernet UDP) ---\r\n");
    if (init_hardware() != XST_SUCCESS) return XST_FAILURE;
    if (setup_network() != 0) return XST_FAILURE;
    start_adc();
    xil_printf("Starting data stream...\r\n");

    while (1) {
        xemacif_input(&echo_netif);

        // Receiving data from FFT (DMA)
        Xil_DCacheInvalidateRange((UINTPTR)fft_buffer, FFT_LENGTH * sizeof(u32));
        Status = XAxiDma_SimpleTransfer(&AxiDma, (UINTPTR)fft_buffer,
                                        FFT_LENGTH * sizeof(u32), XAXIDMA_DEVICE_TO_DMA);
        if (Status != XST_SUCCESS) {
             xil_printf("DMA Error\r\n");
             break;
        }

        // Wait for transfer to complete
        while (XAxiDma_Busy(&AxiDma, XAXIDMA_DEVICE_TO_DMA)) {
        }

        Xil_DCacheInvalidateRange((UINTPTR)fft_buffer, FFT_LENGTH * sizeof(u32));

        // UDP Sending
        send_spectrum_udp(fft_buffer, FFT_LENGTH);
    }

    cleanup_platform();
    return 0;
}