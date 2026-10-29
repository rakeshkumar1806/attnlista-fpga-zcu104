open_project zcu104_system_test/zcu104_system_test.xpr

# Create sim_1 fileset if not present
if {[get_filesets -quiet sim_1] eq ""} {
    create_fileset -simset sim_1
}

# Add tb_system_bd.sv
add_files -fileset sim_1 -norecurse tb_system_bd.sv
set_property top tb_system_bd [get_filesets sim_1]
set_property top_lib xil_defaultlib [get_filesets sim_1]
update_compile_order -fileset sim_1

# Check simulation settings
set_property -name {xsim.simulate.runtime} -value {50ms} -objects [get_filesets sim_1]

# Launch behavioral simulation scripts
launch_simulation -scripts_only

close_project
exit
