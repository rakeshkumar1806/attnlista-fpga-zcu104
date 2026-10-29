open_project -reset fast_sim_prj
set_top attnlista_top
add_files attnlista_top.cpp
add_files cnn_hls_core.cpp
add_files -tb fast_sim.cpp
add_files -tb tb_y_input.txt
add_files -tb tb_cnn_expected.txt
open_solution "solution1" -flow_target vivado
set_part {xczu7ev-ffvc1156-2-e}
create_clock -period 10 -name default
csim_design
exit
