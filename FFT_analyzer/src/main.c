#include <stdio.h>
#include "xparameters.h"
#include "xil_printf.h"
#include "xaxidma.h"
#include "xgpio.h"
#include "xil_cache.h"

#define FFT_LENGTH 1024

// Allocate memory for the spectrum buffer. 
// __attribute__((aligned(32))) is required for proper Zynq L1/L2 cache operations.
u32 fft_buffer[FFT_LENGTH] __attribute__((aligned(32)));

// Global driver instances
XAxiDma AxiDma;
XGpio AdcGpio;

int main()
{
    int Status;

    xil_printf("Spectrometer Operation Started \r\n");

    // AXI GPIO Initialization (ADC Control)
    Status = XGpio_Initialize(&AdcGpio, XPAR_XGPIO_0_BASEADDR);
    if (Status != XST_SUCCESS) {
        xil_printf("GPIO Initialization Failed\r\n");
        return XST_FAILURE;
    }

    // AXI DMA Initialization
    XAxiDma_Config *CfgPtr;
    CfgPtr = XAxiDma_LookupConfig(XPAR_XAXIDMA_0_BASEADDR);
    if (!CfgPtr) {
        xil_printf("DMA Configuration Not Found\r\n");
        return XST_FAILURE;
    }
    Status = XAxiDma_CfgInitialize(&AxiDma, CfgPtr);
    if (Status != XST_SUCCESS) {
        xil_printf("DMA Initialization Failed\r\n");
        return XST_FAILURE;
    }

    // Disable DMA interrupts (operating in Polling mode)
    XAxiDma_IntrDisable(&AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);

    // Start ADC
    xil_printf("Starting ADC...\r\n");
    // Channel 1: Set the frequency divider (e.g., 5)
    XGpio_DiscreteWrite(&AdcGpio, 1, 5); 
    // Channel 2: Enable ADC (enable = 1)
    XGpio_DiscreteWrite(&AdcGpio, 2, 1); 

    // Invalidate the CPU cache to ensure the processor reads new data from RAM instead of data from its own cache
    Xil_DCacheInvalidateRange((UINTPTR)fft_buffer, FFT_LENGTH * sizeof(u32));

    // Trigger DMA to receive data
    xil_printf("Waiting for data from FFT...\r\n");
    Status = XAxiDma_SimpleTransfer(&AxiDma, (UINTPTR)fft_buffer, 
                                    FFT_LENGTH * sizeof(u32), XAXIDMA_DEVICE_TO_DMA);
    if (Status != XST_SUCCESS) {
        xil_printf("DMA Transfer Launch Failed\r\n");
        return XST_FAILURE;
    }

    // Wait for DMA to finish the transfer
    while (XAxiDma_Busy(&AxiDma, XAXIDMA_DEVICE_TO_DMA)) {
        // Poll until the hardware completes the operation
    }

    // Invalidate the cache again after the hardware has written to memory
    Xil_DCacheInvalidateRange((UINTPTR)fft_buffer, FFT_LENGTH * sizeof(u32));
    
    xil_printf("Data Received! Exporting Spectrum:\r\n");
    xil_printf("Index\tReal\tImaginary\r\n");

    // Print data to UART
    for (int i = 0; i < FFT_LENGTH; i++) {
        u32 raw_data = fft_buffer[i];
        short real_part = (short)(raw_data & 0xFFFF);
        short imag_part = (short)((raw_data >> 16) & 0xFFFF);

        // Print via UART: Index, Real value, Imaginary value
        xil_printf("%d\t%d\t%d\r\n", i, real_part, imag_part);
    }

    xil_printf("--- Operation Finished ---\r\n");

    return XST_SUCCESS;
}