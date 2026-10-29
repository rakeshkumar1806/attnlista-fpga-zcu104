open_project -reset lista_prj_synth
set_top attnlista_top
add_files attnlista_top.cpp
open_solution -reset solution1
set_part {xczu7ev-ffvc1156-2-e}
create_clock -period 10 -name default
csynth_design
exit
