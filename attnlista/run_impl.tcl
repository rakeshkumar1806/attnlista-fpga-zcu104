# Create an in-memory project
create_project -in_memory -part xczu7ev-ffvc1156-2-e
set_property board_part xilinx.com:zcu104:part0:1.1 [current_project]
set_property ip_repo_paths { ./lista_prj_synth/solution1/impl/ip ./cnn_prj/solution1/impl/ip } [current_project]
update_ip_catalog

# Read the block design directly
read_bd zcu104_system_test/zcu104_system_test.srcs/sources_1/bd/system_bd/system_bd.bd

# Set to global synthesis so we don't deal with OOC runs
set_property synth_checkpoint_mode None [get_files system_bd.bd]

# Generate targets
generate_target all [get_files system_bd.bd]

# Read wrapper
read_verilog zcu104_system_test/zcu104_system_test.gen/sources_1/bd/system_bd/hdl/system_bd_wrapper.v

# Synthesize
synth_design -top system_bd_wrapper -part xczu7ev-ffvc1156-2-e

# Implement
opt_design
place_design
phys_opt_design
route_design

# Report
report_timing_summary -file post_impl_timing.rpt
report_utilization -file post_impl_utilization.rpt
puts "IMPLEMENTATION SUCCESS"
