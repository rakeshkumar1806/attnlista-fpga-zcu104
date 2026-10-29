open_project -reset lista_prj3
set_top attnlista_top
add_files attnlista_top.cpp
add_files -tb attnlista_tb.cpp
add_files -tb tb_y_input.txt
add_files -tb tb_h_lista_expected.txt
open_solution -reset "solution1"
# ZCU104 (xczu7ev-ffvc1156-2-e). Clock: 10ns (100MHz).
set_part {xczu7ev-ffvc1156-2-e}
create_clock -period 10 -name default

# C simulation
csim_design -clean

# C synthesis

exit
