open_hw_manager
connect_hw_server
set targets [get_hw_targets]
if {[llength $targets] == 0} {
    puts "NO_HARDWARE_TARGET_FOUND"
} else {
    open_hw_target [lindex $targets 0]
    set devices [get_hw_devices]
    puts "HARDWARE_TARGET_FOUND: $devices"
}
