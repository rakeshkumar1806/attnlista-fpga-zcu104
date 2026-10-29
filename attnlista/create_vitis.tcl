setws vitis_workspace
platform create -name hw_plat -hw system_bd_wrapper.xsa -os standalone -proc psu_cortexa53_0
platform generate
app create -name attnlista_app -platform hw_plat -domain standalone_domain -template "Empty Application (C++)"
puts "VITIS PROJECT CREATED"
