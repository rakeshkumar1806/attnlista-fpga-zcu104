create_project -force zcu104_system_test zcu104_system_test -part xczu7ev-ffvc1156-2-e
set_property board_part xilinx.com:zcu104:part0:1.1 [current_project]

set_property ip_repo_paths { ./lista_prj_synth/solution1/impl/ip ./cnn_prj/solution1/impl/ip } [current_project]
update_ip_catalog

create_bd_design "system_bd"

# Add PS
create_bd_cell -type ip -vlnv xilinx.com:ip:zynq_ultra_ps_e:3.* zynq_ultra_ps_e_0
apply_bd_automation -rule xilinx.com:bd_rule:zynq_ultra_ps_e -config {apply_board_preset "1" }  [get_bd_cells zynq_ultra_ps_e_0]

# Add DMA
create_bd_cell -type ip -vlnv xilinx.com:ip:axi_dma:7.* axi_dma_0

# Add HLS IP
create_bd_cell -type ip -vlnv xilinx.com:hls:attnlista_top:1.0 lista_core
create_bd_cell -type ip -vlnv xilinx.com:hls:refinement_cnn_hls:1.0 cnn_core

# Try to connect streams directly to see the warnings/errors
connect_bd_intf_net [get_bd_intf_pins lista_core/h_out] [get_bd_intf_pins cnn_core/refine_input]

validate_bd_design
