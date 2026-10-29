#include "xparameters.h"
#include "xaxidma.h"
#include "xattnlista_top.h"
#include "xrefinement_cnn_hls.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "test_vectors.h"

// DMA Instances
XAxiDma dma_lista_y_out;
XAxiDma dma_lista_init;
XAxiDma dma_cnn_refine_out;
XAxiDma dma_cnn_refine_2;
XAxiDma dma_cnn_res;

// HLS Core Instances
XAttnlista_top lista_core;
XRefinement_cnn_hls cnn_core;

// DDR Memory Buffers
#define MEM_BASE_ADDR 0x10000000
float* rx_lista_h_out = (float*)(MEM_BASE_ADDR);
float* rx_lista_h_ls_init = (float*)(MEM_BASE_ADDR + 0x10000);
float* rx_cnn_out = (float*)(MEM_BASE_ADDR + 0x20000);

void init_dma(XAxiDma* dma_inst, u16 device_id) {
    XAxiDma_Config *CfgPtr = XAxiDma_LookupConfig(device_id);
    XAxiDma_CfgInitialize(dma_inst, CfgPtr);
    XAxiDma_IntrDisable(dma_inst, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);
    XAxiDma_IntrDisable(dma_inst, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);
}

int main() {
    xil_printf("\r\n--- AttnLISTA ZCU104 Hardware Execution ---\r\n");

    // 1. Initialize HLS Cores
    XAttnlista_top_Initialize(&lista_core, XPAR_ATTNLISTA_TOP_0_DEVICE_ID);
    XRefinement_cnn_hls_Initialize(&cnn_core, XPAR_REFINEMENT_CNN_HLS_0_DEVICE_ID);

    // 2. Initialize DMAs
    // (Note: device IDs are automatically generated in xparameters.h based on the block design)
    init_dma(&dma_lista_y_out, XPAR_AXIDMA_0_DEVICE_ID);
    init_dma(&dma_lista_init, XPAR_AXIDMA_1_DEVICE_ID);
    init_dma(&dma_cnn_refine_out, XPAR_AXIDMA_2_DEVICE_ID);
    init_dma(&dma_cnn_refine_2, XPAR_AXIDMA_3_DEVICE_ID);
    init_dma(&dma_cnn_res, XPAR_AXIDMA_4_DEVICE_ID);

    // 3. Setup LISTA Transfers
    Xil_DCacheFlushRange((UINTPTR)tb_y_input, sizeof(tb_y_input));
    
    // Arm LISTA RX DMAs (S2MM)
    XAxiDma_SimpleTransfer(&dma_lista_y_out, (UINTPTR)rx_lista_h_out, 16*65*8, XAXIDMA_DEVICE_TO_DMA);
    XAxiDma_SimpleTransfer(&dma_lista_init, (UINTPTR)rx_lista_h_ls_init, 16*65*8, XAXIDMA_DEVICE_TO_DMA);
    
    // Start LISTA Core
    XAttnlista_top_Start(&lista_core);
    
    // Trigger LISTA TX DMA (MM2S)
    XAxiDma_SimpleTransfer(&dma_lista_y_out, (UINTPTR)tb_y_input, sizeof(tb_y_input), XAXIDMA_DMA_TO_DEVICE);

    // Wait for LISTA completion
    while (XAxiDma_Busy(&dma_lista_y_out, XAXIDMA_DMA_TO_DEVICE)) {}
    while (XAxiDma_Busy(&dma_lista_y_out, XAXIDMA_DEVICE_TO_DMA)) {}
    while (XAxiDma_Busy(&dma_lista_init, XAXIDMA_DEVICE_TO_DMA)) {}
    while (!XAttnlista_top_IsDone(&lista_core)) {}

    xil_printf("LISTA core finished.\r\n");
    Xil_DCacheInvalidateRange((UINTPTR)rx_lista_h_out, 16*65*8);
    Xil_DCacheInvalidateRange((UINTPTR)rx_lista_h_ls_init, 16*65*8);

    // 4. Setup CNN Transfers
    Xil_DCacheFlushRange((UINTPTR)rx_lista_h_out, 16*65*8);
    Xil_DCacheFlushRange((UINTPTR)rx_lista_h_ls_init, 16*65*8);

    // Arm CNN RX DMA (S2MM)
    XAxiDma_SimpleTransfer(&dma_cnn_refine_out, (UINTPTR)rx_cnn_out, 16*65*8, XAXIDMA_DEVICE_TO_DMA);

    // Start CNN Core
    XRefinement_cnn_hls_Start(&cnn_core);

    // Trigger CNN TX DMAs (MM2S)
    // refine_input_1 gets LISTA H_out
    XAxiDma_SimpleTransfer(&dma_cnn_refine_out, (UINTPTR)rx_lista_h_out, 16*65*8, XAXIDMA_DMA_TO_DEVICE);
    // refine_input_2 gets LISTA H_ls_init
    XAxiDma_SimpleTransfer(&dma_cnn_refine_2, (UINTPTR)rx_lista_h_ls_init, 16*65*8, XAXIDMA_DMA_TO_DEVICE);
    // residual_base gets LISTA H_out again (for the final addition)
    XAxiDma_SimpleTransfer(&dma_cnn_res, (UINTPTR)rx_lista_h_out, 16*65*8, XAXIDMA_DMA_TO_DEVICE);

    // Wait for CNN completion
    while (XAxiDma_Busy(&dma_cnn_refine_out, XAXIDMA_DMA_TO_DEVICE)) {}
    while (XAxiDma_Busy(&dma_cnn_refine_2, XAXIDMA_DMA_TO_DEVICE)) {}
    while (XAxiDma_Busy(&dma_cnn_res, XAXIDMA_DMA_TO_DEVICE)) {}
    while (XAxiDma_Busy(&dma_cnn_refine_out, XAXIDMA_DEVICE_TO_DMA)) {}
    while (!XRefinement_cnn_hls_IsDone(&cnn_core)) {}

    xil_printf("CNN core finished.\r\n");
    Xil_DCacheInvalidateRange((UINTPTR)rx_cnn_out, 16*65*8);

    // 5. Compute Hardware NMSE
    float err_sum = 0.0f;
    float norm_sum = 0.0f;
    for (int i = 0; i < 16*65*2; i++) {
        float hw_val = rx_cnn_out[i];
        float expected = tb_cnn_expected[i];
        float diff = hw_val - expected;
        err_sum += diff * diff;
        norm_sum += expected * expected;
    }
    
    float nmse = err_sum / norm_sum;
    xil_printf("Final Hardware NMSE vs Golden Model: %f\r\n", nmse);
    xil_printf("--- Done ---\r\n");
    
    return 0;
}
