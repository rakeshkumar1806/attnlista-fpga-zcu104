create_project -force zcu104_system_test zcu104_system_test -part xczu7ev-ffvc1156-2-e
set_property board_part xilinx.com:zcu104:part0:1.1 [current_project]

set_property ip_repo_paths { ./lista_prj_synth/solution1/impl/ip ./cnn_prj/solution1/impl/ip } [current_project]
update_ip_catalog

create_bd_design "system_bd"

# Add PS
create_bd_cell -type ip -vlnv xilinx.com:ip:zynq_ultra_ps_e:3.* zynq_ultra_ps_e_0
apply_bd_automation -rule xilinx.com:bd_rule:zynq_ultra_ps_e -config {apply_board_preset "1" }  [get_bd_cells zynq_ultra_ps_e_0]
set_property CONFIG.PSU__USE__S_AXI_GP2 {1} [get_bd_cells zynq_ultra_ps_e_0]
set_property CONFIG.PSU__USE__S_AXI_GP3 {1} [get_bd_cells zynq_ultra_ps_e_0]

# Add HLS IPs
create_bd_cell -type ip -vlnv xilinx.com:hls:attnlista_top:1.0 lista_core
create_bd_cell -type ip -vlnv xilinx.com:hls:refinement_cnn_hls:1.0 cnn_core

# Add DMAs
create_bd_cell -type ip -vlnv xilinx.com:ip:axi_dma:7.* dma_lista_y_out
set_property -dict [list CONFIG.c_include_sg {0} CONFIG.c_m_axi_mm2s_data_width {64} CONFIG.c_m_axis_mm2s_tdata_width {64} CONFIG.c_m_axi_s2mm_data_width {64} CONFIG.c_s_axis_s2mm_tdata_width {64}] [get_bd_cells dma_lista_y_out]

create_bd_cell -type ip -vlnv xilinx.com:ip:axi_dma:7.* dma_lista_init
set_property -dict [list CONFIG.c_include_sg {0} CONFIG.c_include_mm2s {0} CONFIG.c_m_axi_s2mm_data_width {64} CONFIG.c_s_axis_s2mm_tdata_width {64}] [get_bd_cells dma_lista_init]

create_bd_cell -type ip -vlnv xilinx.com:ip:axi_dma:7.* dma_cnn_refine_out
set_property -dict [list CONFIG.c_include_sg {0} CONFIG.c_m_axi_mm2s_data_width {64} CONFIG.c_m_axis_mm2s_tdata_width {64} CONFIG.c_m_axi_s2mm_data_width {64} CONFIG.c_s_axis_s2mm_tdata_width {64}] [get_bd_cells dma_cnn_refine_out]

create_bd_cell -type ip -vlnv xilinx.com:ip:axi_dma:7.* dma_cnn_refine_2
set_property -dict [list CONFIG.c_include_sg {0} CONFIG.c_include_s2mm {0} CONFIG.c_m_axi_mm2s_data_width {64} CONFIG.c_m_axis_mm2s_tdata_width {64}] [get_bd_cells dma_cnn_refine_2]

create_bd_cell -type ip -vlnv xilinx.com:ip:axi_dma:7.* dma_cnn_res
set_property -dict [list CONFIG.c_include_sg {0} CONFIG.c_include_s2mm {0} CONFIG.c_m_axi_mm2s_data_width {64} CONFIG.c_m_axis_mm2s_tdata_width {64}] [get_bd_cells dma_cnn_res]

# Connect Streams
connect_bd_intf_net [get_bd_intf_pins dma_lista_y_out/M_AXIS_MM2S] [get_bd_intf_pins lista_core/y]
connect_bd_intf_net [get_bd_intf_pins lista_core/h_out] [get_bd_intf_pins dma_lista_y_out/S_AXIS_S2MM]
connect_bd_intf_net [get_bd_intf_pins lista_core/h_ls_init_out] [get_bd_intf_pins dma_lista_init/S_AXIS_S2MM]

connect_bd_intf_net [get_bd_intf_pins dma_cnn_refine_out/M_AXIS_MM2S] [get_bd_intf_pins cnn_core/refine_input_1]
connect_bd_intf_net [get_bd_intf_pins dma_cnn_refine_2/M_AXIS_MM2S] [get_bd_intf_pins cnn_core/refine_input_2]
connect_bd_intf_net [get_bd_intf_pins cnn_core/out_r] [get_bd_intf_pins dma_cnn_refine_out/S_AXIS_S2MM]
connect_bd_intf_net [get_bd_intf_pins dma_cnn_res/M_AXIS_MM2S] [get_bd_intf_pins cnn_core/residual_base]

# Run Connection Automation for Clocks, Resets, AXI-Lite and AXI-MM
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {/zynq_ultra_ps_e_0/pl_clk0 (99 MHz)} Clk_slave {Auto} Clk_xbar {Auto} Master {/zynq_ultra_ps_e_0/M_AXI_HPM0_FPD} Slave {/lista_core/s_axi_control} ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0}}  [get_bd_intf_pins lista_core/s_axi_control]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {/zynq_ultra_ps_e_0/pl_clk0 (99 MHz)} Clk_slave {Auto} Clk_xbar {Auto} Master {/zynq_ultra_ps_e_0/M_AXI_HPM0_FPD} Slave {/cnn_core/s_axi_control} ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0}}  [get_bd_intf_pins cnn_core/s_axi_control]

apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {/zynq_ultra_ps_e_0/pl_clk0 (99 MHz)} Clk_slave {Auto} Clk_xbar {Auto} Master {/zynq_ultra_ps_e_0/M_AXI_HPM0_FPD} Slave {/dma_lista_y_out/S_AXI_LITE} ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0}}  [get_bd_intf_pins dma_lista_y_out/S_AXI_LITE]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {/zynq_ultra_ps_e_0/pl_clk0 (99 MHz)} Clk_slave {Auto} Clk_xbar {Auto} Master {/zynq_ultra_ps_e_0/M_AXI_HPM0_FPD} Slave {/dma_lista_init/S_AXI_LITE} ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0}}  [get_bd_intf_pins dma_lista_init/S_AXI_LITE]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {/zynq_ultra_ps_e_0/pl_clk0 (99 MHz)} Clk_slave {Auto} Clk_xbar {Auto} Master {/zynq_ultra_ps_e_0/M_AXI_HPM0_FPD} Slave {/dma_cnn_refine_out/S_AXI_LITE} ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0}}  [get_bd_intf_pins dma_cnn_refine_out/S_AXI_LITE]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {/zynq_ultra_ps_e_0/pl_clk0 (99 MHz)} Clk_slave {Auto} Clk_xbar {Auto} Master {/zynq_ultra_ps_e_0/M_AXI_HPM0_FPD} Slave {/dma_cnn_refine_2/S_AXI_LITE} ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0}}  [get_bd_intf_pins dma_cnn_refine_2/S_AXI_LITE]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {/zynq_ultra_ps_e_0/pl_clk0 (99 MHz)} Clk_slave {Auto} Clk_xbar {Auto} Master {/zynq_ultra_ps_e_0/M_AXI_HPM0_FPD} Slave {/dma_cnn_res/S_AXI_LITE} ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0}}  [get_bd_intf_pins dma_cnn_res/S_AXI_LITE]

apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {/zynq_ultra_ps_e_0/pl_clk0 (99 MHz)} Clk_xbar {/zynq_ultra_ps_e_0/pl_clk0 (99 MHz)} Master {/dma_lista_y_out/M_AXI_MM2S} Slave {/zynq_ultra_ps_e_0/S_AXI_HP0_FPD} ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0}}  [get_bd_intf_pins zynq_ultra_ps_e_0/S_AXI_HP0_FPD]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {/zynq_ultra_ps_e_0/pl_clk0 (99 MHz)} Clk_xbar {/zynq_ultra_ps_e_0/pl_clk0 (99 MHz)} Master {/dma_lista_y_out/M_AXI_S2MM} Slave {/zynq_ultra_ps_e_0/S_AXI_HP0_FPD} ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0}}  [get_bd_intf_pins dma_lista_y_out/M_AXI_S2MM]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {/zynq_ultra_ps_e_0/pl_clk0 (99 MHz)} Clk_xbar {/zynq_ultra_ps_e_0/pl_clk0 (99 MHz)} Master {/dma_lista_init/M_AXI_S2MM} Slave {/zynq_ultra_ps_e_0/S_AXI_HP0_FPD} ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0}}  [get_bd_intf_pins dma_lista_init/M_AXI_S2MM]

apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {/zynq_ultra_ps_e_0/pl_clk0 (99 MHz)} Clk_xbar {/zynq_ultra_ps_e_0/pl_clk0 (99 MHz)} Master {/dma_cnn_refine_out/M_AXI_MM2S} Slave {/zynq_ultra_ps_e_0/S_AXI_HP1_FPD} ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0}}  [get_bd_intf_pins zynq_ultra_ps_e_0/S_AXI_HP1_FPD]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {/zynq_ultra_ps_e_0/pl_clk0 (99 MHz)} Clk_xbar {/zynq_ultra_ps_e_0/pl_clk0 (99 MHz)} Master {/dma_cnn_refine_2/M_AXI_MM2S} Slave {/zynq_ultra_ps_e_0/S_AXI_HP1_FPD} ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0}}  [get_bd_intf_pins dma_cnn_refine_2/M_AXI_MM2S]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {/zynq_ultra_ps_e_0/pl_clk0 (99 MHz)} Clk_xbar {/zynq_ultra_ps_e_0/pl_clk0 (99 MHz)} Master {/dma_cnn_refine_out/M_AXI_S2MM} Slave {/zynq_ultra_ps_e_0/S_AXI_HP1_FPD} ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0}}  [get_bd_intf_pins dma_cnn_refine_out/M_AXI_S2MM]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {/zynq_ultra_ps_e_0/pl_clk0 (99 MHz)} Clk_xbar {/zynq_ultra_ps_e_0/pl_clk0 (99 MHz)} Master {/dma_cnn_res/M_AXI_MM2S} Slave {/zynq_ultra_ps_e_0/S_AXI_HP1_FPD} ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0}}  [get_bd_intf_pins dma_cnn_res/M_AXI_MM2S]
connect_bd_net [get_bd_pins zynq_ultra_ps_e_0/maxihpm1_fpd_aclk] [get_bd_pins zynq_ultra_ps_e_0/pl_clk0]

validate_bd_design
save_bd_design
puts "VALIDATION SUCCESS"

set wrapper_path [make_wrapper -files [get_files system_bd.bd] -top]
add_files -norecurse $wrapper_path
set_property top system_bd_wrapper [current_fileset]
update_compile_order -fileset sources_1

launch_runs impl_1 -jobs 4
wait_on_run impl_1

open_run impl_1
report_timing_summary -file post_impl_timing.rpt
report_utilization -file post_impl_utilization.rpt
puts "IMPLEMENTATION SUCCESS"
