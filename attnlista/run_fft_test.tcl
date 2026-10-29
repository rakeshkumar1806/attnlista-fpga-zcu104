open_project -reset fft_prj
set_top radix2_fft_rows
add_files -tb fft_test.cpp
add_files -tb tb_fft_16_in.txt
add_files -tb tb_fft_16_out.txt
open_solution -reset "solution1"
set_part {xczu7ev-ffvc1156-2-e}
create_clock -period 10 -name default
csim_design
exit
