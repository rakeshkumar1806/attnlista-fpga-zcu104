open_project zcu104_system_test/zcu104_system_test.xpr
generate_target simulation [get_files system_bd.bd]
export_ip_user_files -of_objects [get_files system_bd.bd] -no_script -sync -force -quiet
close_project
exit
