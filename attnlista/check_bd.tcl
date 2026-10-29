open_project zcu104_system_test/zcu104_system_test.xpr
open_bd_design [get_files system_bd.bd]
set ips [get_bd_cells -hierarchical]
puts "--- BD Cells ---"
foreach ip $ips {
    puts "Cell: $ip, VLNV: [get_property VLNV [get_bd_cells $ip]]"
}
puts "--- End BD Cells ---"
puts "--- IP Status ---"
report_ip_status
close_project
exit
