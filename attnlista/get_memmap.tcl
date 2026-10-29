open_project zcu104_system_test/zcu104_system_test.xpr
open_bd_design zcu104_system_test/zcu104_system_test.srcs/sources_1/bd/system_bd/system_bd.bd

foreach seg [get_bd_addr_segs -of_objects [get_bd_addr_spaces zynq_ultra_ps_e_0/Data]] {
    puts "ADDR_MAP: $seg : [get_property offset $seg] -> [get_property range $seg]"
}
