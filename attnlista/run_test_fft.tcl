open_project test_fft_prj
set_top main
add_files -tb test_fft.cpp -cflags "-std=c++11"
open_solution solution1
csim_design
exit
