open_project zcu104_system_test/zcu104_system_test.xpr
launch_runs impl_1 -to_step write_bitstream -jobs 4
wait_on_run impl_1
puts "BITSTREAM GENERATION COMPLETE"
