# build.tcl
create_project -force zcu104_system_test zcu104_system_test -part xczu7ev-ffvc1156-2-e
set_property board_part xilinx.com:zcu104:part0:1.1 [current_project]
set_property ip_repo_paths { ./lista_prj_synth/solution1/impl/ip ./cnn_prj/solution1/impl/ip } [current_project]
update_ip_catalog

# Run the BD creation script
source build_final_bd.tcl

# BD is now created. Generate wrapper
set wrapper_path [make_wrapper -files [get_files system_bd.bd] -top]
add_files -norecurse $wrapper_path
set_property top system_bd_wrapper [current_fileset]
update_compile_order -fileset sources_1

# Launch implementation
launch_runs impl_1 -jobs 4
wait_on_run impl_1

open_run impl_1
report_timing_summary -file post_impl_timing.rpt
report_utilization -file post_impl_utilization.rpt
puts "IMPLEMENTATION SUCCESS"
