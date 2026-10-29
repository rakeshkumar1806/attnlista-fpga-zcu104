open_hw_manager
connect_hw_server
open_hw_target [lindex [get_hw_targets] 0]
set device [lindex [get_hw_devices] 0]
current_hw_device $device

set_property PROGRAM.FILE {C:/Users/jbvrr/.gemini/antigravity/scratch/attnlista/zcu104_system_test/zcu104_system_test.runs/impl_1/system_bd_wrapper.bit} $device
puts "PROGRAMMING DEVICE..."
program_hw_devices $device
refresh_hw_device $device

set done_status [get_property REGISTER.IR.BIT14_DONE $device]
puts "PROGRAMMING COMPLETED"
puts "DONE PIN STATUS: $done_status"
