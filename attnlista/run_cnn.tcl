open_project -reset cnn_prj
set_top refinement_cnn_hls
add_files cnn_hls_core.cpp
add_files -tb cnn_tb.cpp
add_files -tb tb_cnn_input.txt
add_files -tb tb_cnn_residual_base.txt
add_files -tb tb_cnn_expected.txt
open_solution -reset solution1
set_part {xczu7ev-ffvc1156-2-e}
create_clock -period 10 -name default
csim_design
csynth_design
exit
