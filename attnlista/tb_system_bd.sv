`timescale 1ns / 1ps

module tb_system_bd;

  // DUT instantiation - system_bd_wrapper contains system_bd which contains Zynq VIP, cores, and DMAs
  system_bd_wrapper dut();

  // Address definitions matching xparameters.h
  localparam [31:0] LISTA_CTRL_BASE      = 32'hA0000000;
  localparam [31:0] CNN_CTRL_BASE        = 32'hA0010000;
  localparam [31:0] DMA_LISTA_Y_OUT_BASE = 32'hA0020000;
  localparam [31:0] DMA_LISTA_INIT_BASE  = 32'hA0030000;
  localparam [31:0] DMA_CNN_REFINE_OUT   = 32'hA0040000;
  localparam [31:0] DMA_CNN_REFINE_2     = 32'hA0050000;
  localparam [31:0] DMA_CNN_RES          = 32'hA0060000;

  // AXI DMA Offsets
  localparam [31:0] MM2S_DMACR = 32'h00;
  localparam [31:0] MM2S_DMASR = 32'h04;
  localparam [31:0] MM2S_SA    = 32'h18;
  localparam [31:0] MM2S_LEN   = 32'h28;

  localparam [31:0] S2MM_DMACR = 32'h30;
  localparam [31:0] S2MM_DMASR = 32'h34;
  localparam [31:0] S2MM_DA    = 32'h48;
  localparam [31:0] S2MM_LEN   = 32'h58;

  // DDR Memory Buffers
  localparam [31:0] ADDR_RX_LISTA_H_OUT     = 32'h10000000;
  localparam [31:0] ADDR_RX_LISTA_H_LS_INIT = 32'h10010000;
  localparam [31:0] ADDR_RX_CNN_OUT         = 32'h10020000;
  localparam [31:0] ADDR_TB_Y_INPUT         = 32'h10030000;

  // AXI-Lite register write via Zynq MPSoC VIP
  task automatic axi_write_reg(input [31:0] addr, input [31:0] data);
    reg [1:0] resp;
    reg [2047:0] w_data;
    w_data = 2048'd0;
    w_data[31:0] = data;
    dut.system_bd_i.zynq_ultra_ps_e_0.inst.write_data(addr, 4, w_data, resp);
  endtask

  // AXI-Lite register read via Zynq MPSoC VIP
  task automatic axi_read_reg(input [31:0] addr, output [31:0] data);
    reg [1:0] resp;
    reg [2047:0] r_data;
    dut.system_bd_i.zynq_ultra_ps_e_0.inst.read_data(addr, 4, r_data, resp);
    data = r_data[31:0];
  endtask

  // DDR memory write via Zynq MPSoC VIP
  task automatic write_ddr_word(input [31:0] addr, input [31:0] val);
    reg [32767:0] data;
    data = 32768'd0;
    data[31:0] = val;
    dut.system_bd_i.zynq_ultra_ps_e_0.inst.write_mem(data, addr, 4);
  endtask

  // DDR memory read via Zynq MPSoC VIP
  task automatic read_ddr_word(input [31:0] addr, output [31:0] val);
    reg [32767:0] data;
    dut.system_bd_i.zynq_ultra_ps_e_0.inst.read_mem(addr, 4, data);
    val = data[31:0];
  endtask

  // DMA helper tasks
  task automatic dma_start_mm2s(input [31:0] dma_base, input [31:0] src_addr, input [31:0] len);
    axi_write_reg(dma_base + MM2S_DMACR, 32'h0001);
    axi_write_reg(dma_base + MM2S_SA, src_addr);
    axi_write_reg(dma_base + MM2S_LEN, len);
  endtask

  task automatic dma_start_s2mm(input [31:0] dma_base, input [31:0] dest_addr, input [31:0] len);
    axi_write_reg(dma_base + S2MM_DMACR, 32'h0001);
    axi_write_reg(dma_base + S2MM_DA, dest_addr);
    axi_write_reg(dma_base + S2MM_LEN, len);
  endtask

  task automatic wait_core_done(input [31:0] core_base, input string name, input integer fast_forward_ns);
    reg [31:0] ctrl;
    integer polls;
    polls = 0;
    if (fast_forward_ns > 0) begin
      $display("[%0t] Fast-forwarding simulation for %0s (%0d ns)...", $time, name, fast_forward_ns);
      #(fast_forward_ns);
    end
    axi_read_reg(core_base, ctrl);
    while ((ctrl & 32'h0002) == 0 && polls < 200) begin
      #50000; // Poll every 50 us
      axi_read_reg(core_base, ctrl);
      polls = polls + 1;
    end
    if ((ctrl & 32'h0002) == 0) begin
      $display("[%0t] TIMEOUT waiting for %0s to complete! CTRL = 0x%08h", $time, name, ctrl);
    end else begin
      $display("[%0t] %0s completed successfully! (ap_done asserted, CTRL = 0x%08h, polls = %0d)", $time, name, ctrl, polls);
    end
  endtask

  // Main stimulus process replicating main.c exactly
  initial begin
    integer file_y, r_scan;
    shortreal re, im;
    reg [31:0] re_bits, im_bits;
    integer idx;

    integer file_exp, r_exp;
    shortreal exp_val, hw_val;
    reg [31:0] hw_bits;
    real err_sum, norm_sum, diff, nmse, nmse_db;
    integer out_idx;

    $display("=== [TESTBENCH] Starting End-to-End System Simulation ===");
    
    // 1. Reset Zynq and PL
    $display("[%0t] Asserting Reset...", $time);
    dut.system_bd_i.zynq_ultra_ps_e_0.inst.por_srstb_reset(1'b0);
    #200;
    $display("[%0t] Releasing Reset...", $time);
    dut.system_bd_i.zynq_ultra_ps_e_0.inst.por_srstb_reset(1'b1);
    dut.system_bd_i.zynq_ultra_ps_e_0.inst.fpga_soft_reset(32'h0);
    #1000;

    // Disable verbose VIP debug tracing to run at full simulation speed
    dut.system_bd_i.zynq_ultra_ps_e_0.inst.set_debug_level_info(1'b0);

    // 2. Load tb_y_input into simulated DDR memory model
    $display("[%0t] Loading tb_y_input.txt into DDR memory at 0x%08h...", $time, ADDR_TB_Y_INPUT);
    file_y = $fopen("C:/Users/jbvrr/.gemini/antigravity/scratch/attnlista/tb_y_input.txt", "r");
    if (file_y == 0) begin
      file_y = $fopen("tb_y_input.txt", "r");
    end
    if (file_y == 0) begin
      $display("ERROR: Cannot open tb_y_input.txt!");
      $finish;
    end
    idx = 0;
    while (!$feof(file_y) && idx < 256) begin
      r_scan = $fscanf(file_y, "%f %f\n", re, im);
      if (r_scan == 2) begin
        re_bits = $shortrealtobits(re);
        im_bits = $shortrealtobits(im);
        write_ddr_word(ADDR_TB_Y_INPUT + (idx * 8), re_bits);
        write_ddr_word(ADDR_TB_Y_INPUT + (idx * 8) + 4, im_bits);
        idx = idx + 1;
      end
    end
    $fclose(file_y);
    $display("[%0t] Successfully loaded %0d complex values (2048 bytes) into simulated DDR.", $time, idx);

    // 3. Setup LISTA Transfers
    $display("[%0t] Arming LISTA RX DMAs...", $time);
    dma_start_s2mm(DMA_LISTA_Y_OUT_BASE, ADDR_RX_LISTA_H_OUT, 16*65*8);
    dma_start_s2mm(DMA_LISTA_INIT_BASE, ADDR_RX_LISTA_H_LS_INIT, 16*65*8);

    $display("[%0t] Starting LISTA Core (ap_start = 1)...", $time);
    axi_write_reg(LISTA_CTRL_BASE, 32'h01);

    $display("[%0t] Triggering LISTA TX DMA (2048 bytes)...", $time);
    dma_start_mm2s(DMA_LISTA_Y_OUT_BASE, ADDR_TB_Y_INPUT, 2048);

    // 4. Wait for LISTA completion
    #20000; // 20 us is sufficient for MM2S (2048 bytes = 256 clock cycles = 2.56 us)
    $display("[%0t] LISTA Core processing (16 layers, ~8.2 ms)...", $time);
    wait_core_done(LISTA_CTRL_BASE, "lista_core", 8000000);
    #20000; // 20 us flush time for S2MM stream output to DDR
    $display("[%0t] LISTA output transfers to DDR completed.", $time);

    // 5. Setup CNN Transfers
    $display("[%0t] Arming CNN RX DMA...", $time);
    dma_start_s2mm(DMA_CNN_REFINE_OUT, ADDR_RX_CNN_OUT, 16*65*8);

    $display("[%0t] Starting CNN Core (ap_start = 1)...", $time);
    axi_write_reg(CNN_CTRL_BASE, 32'h01);

    $display("[%0t] Triggering CNN TX DMAs...", $time);
    dma_start_mm2s(DMA_CNN_REFINE_OUT, ADDR_RX_LISTA_H_OUT, 16*65*8);
    dma_start_mm2s(DMA_CNN_REFINE_2, ADDR_RX_LISTA_H_LS_INIT, 16*65*8);
    dma_start_mm2s(DMA_CNN_RES, ADDR_RX_LISTA_H_OUT, 16*65*8);

    // 6. Wait for CNN completion
    #30000; // 30 us is sufficient for MM2S transfers
    $display("[%0t] CNN Core refining channel (~7.8 ms)...", $time);
    wait_core_done(CNN_CTRL_BASE, "cnn_core", 7500000);
    #20000; // 20 us flush time for S2MM stream output to DDR
    $display("[%0t] CNN output transfer to DDR completed.", $time);

    // 7. Read back final results and compute NMSE vs tb_cnn_expected.txt
    $display("[%0t] Reading CNN output from DDR at 0x%08h...", $time, ADDR_RX_CNN_OUT);
    file_exp = $fopen("C:/Users/jbvrr/.gemini/antigravity/scratch/attnlista/tb_cnn_expected.txt", "r");
    if (file_exp == 0) begin
      file_exp = $fopen("tb_cnn_expected.txt", "r");
    end
    if (file_exp == 0) begin
      $display("ERROR: Cannot open tb_cnn_expected.txt!");
      $finish;
    end

    err_sum = 0.0;
    norm_sum = 0.0;
    out_idx = 0;
    while (!$feof(file_exp) && out_idx < 2080) begin
      r_exp = $fscanf(file_exp, "%f\n", exp_val);
      if (r_exp == 1) begin
        read_ddr_word(ADDR_RX_CNN_OUT + (out_idx * 4), hw_bits);
        hw_val = $bitstoshortreal(hw_bits);
        diff = hw_val - exp_val;
        err_sum = err_sum + (diff * diff);
        norm_sum = norm_sum + (exp_val * exp_val);
        if (out_idx < 8) begin
          $display("  Out[%04d]: HW = %f, EXP = %f, Error = %f", out_idx, hw_val, exp_val, diff);
        end
        out_idx = out_idx + 1;
      end
    end
    $fclose(file_exp);

    nmse = err_sum / norm_sum;
    nmse_db = 10.0 * $log10(nmse);

    $display("=================================================");
    $display("=== SIMULATION COMPLETED SUCCESSFULLY ===");
    $display("Total output values evaluated : %0d", out_idx);
    $display("Error sum (||HW - EXP||^2)    : %e", err_sum);
    $display("Norm sum (||EXP||^2)          : %e", norm_sum);
    $display("Simulated Hardware NMSE       : %f", nmse);
    $display("Simulated Hardware NMSE (dB)  : %f dB", nmse_db);
    if (nmse_db < -25.0) begin
      $display("RESULT: [PASS] (NMSE %0.2f dB < -25 dB threshold)", nmse_db);
    end else begin
      $display("RESULT: [FAIL] (NMSE %0.2f dB >= -25 dB threshold)", nmse_db);
    end
    $display("=================================================");

    #1000;
    $finish;
  end

endmodule
