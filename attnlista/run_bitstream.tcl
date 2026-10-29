open_project vivado_prj/zcu104_attnlista.xpr
open_run impl_1
set_property BITSTREAM.General.UnconstrainedPins {Allow} [current_design]
write_bitstream -force vivado_prj/attnlista_bd_wrapper.bit
exit
