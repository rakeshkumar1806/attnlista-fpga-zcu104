open_project vivado_prj/zcu104_attnlista.xpr
open_run impl_1
read_xdc timing.xdc
report_timing_summary -file post_impl_timing_constrained.txt
exit
