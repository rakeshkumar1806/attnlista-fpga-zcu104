open_project -reset cnn_export_prj
set_top refinement_cnn_hls
add_files cnn_hls_core.cpp
open_solution -reset solution1
set_part {xczu7ev-ffvc1156-2-e}
create_clock -period 10 -name default
csynth_design
export_design -format ip_catalog
exit
