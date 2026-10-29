create_project -force zcu104_attnlista vivado_prj -part xczu7ev-ffvc1156-2-e
set_property board_part xilinx.com:zcu104:part0:1.1 [current_project]

# Set IP Repository Paths
set_property ip_repo_paths { ./lista_prj_synth/solution1/impl/ip ./cnn_prj/solution1/impl/ip } [current_project]
update_ip_catalog

# Create Block Design
create_bd_design "attnlista_bd"

# Instantiate IPs
create_bd_cell -type ip -vlnv xilinx.com:hls:attnlista_top:1.0 lista_core
create_bd_cell -type ip -vlnv xilinx.com:hls:refinement_cnn_hls:1.0 cnn_core

# Add clock and reset
create_bd_port -dir I -type clk -freq_hz 100000000 ap_clk
create_bd_port -dir I -type rst ap_rst_n

# Connect clocks
connect_bd_net [get_bd_ports ap_clk] [get_bd_pins lista_core/ap_clk]
connect_bd_net [get_bd_ports ap_clk] [get_bd_pins cnn_core/ap_clk]

# Connect resets
connect_bd_net [get_bd_ports ap_rst_n] [get_bd_pins lista_core/ap_rst_n]
connect_bd_net [get_bd_ports ap_rst_n] [get_bd_pins cnn_core/ap_rst_n]

# Make all other interfaces external so they aren't optimized away
make_bd_intf_pins_external [get_bd_intf_pins lista_core/*]
make_bd_intf_pins_external [get_bd_intf_pins cnn_core/*]

# Create HDL wrapper
make_wrapper -files [get_files vivado_prj/zcu104_attnlista.srcs/sources_1/bd/attnlista_bd/attnlista_bd.bd] -top
add_files -norecurse vivado_prj/zcu104_attnlista.gen/sources_1/bd/attnlista_bd/hdl/attnlista_bd_wrapper.v
set_property top attnlista_bd_wrapper [current_fileset]
update_compile_order -fileset sources_1

# Synthesize and Implement (OOC mode for the block design is default)
# We will run synthesis and implementation to get real utilization
set_property -name {STEPS.SYNTH_DESIGN.ARGS.MORE OPTIONS} -value {-mode out_of_context} -objects [get_runs synth_1]
launch_runs synth_1 -jobs 8
wait_on_run synth_1

launch_runs impl_1 -jobs 8
wait_on_run impl_1

open_run impl_1
report_utilization -file post_impl_utilization.txt
report_timing_summary -file post_impl_timing.txt

exit
