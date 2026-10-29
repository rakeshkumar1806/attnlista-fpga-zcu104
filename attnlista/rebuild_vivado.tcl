open_project zcu104_system_test/zcu104_system_test.xpr
open_bd_design [get_files system_bd.bd]
update_ip_catalog -rebuild -scan_changes
upgrade_bd_cells [get_bd_cells lista_core]
upgrade_bd_cells [get_bd_cells cnn_core]
validate_bd_design
save_bd_design
reset_run synth_1
launch_runs impl_1 -to_step write_bitstream -jobs 8
wait_on_run impl_1
write_hw_platform -fixed -include_bit -force -file system_bd_wrapper.xsa
exit
