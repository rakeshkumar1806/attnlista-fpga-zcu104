open_project zcu104_system_test/zcu104_system_test.xpr
open_bd_design [get_files system_bd.bd]

set lista_w [get_property CONFIG.TDATA_NUM_BYTES [get_bd_intf_pins lista_core/h_out]]
set cnn_w_in [get_property CONFIG.TDATA_NUM_BYTES [get_bd_intf_pins cnn_core/refine_input]]
set cnn_w_res [get_property CONFIG.TDATA_NUM_BYTES [get_bd_intf_pins cnn_core/residual_base]]
set cnn_w_out [get_property CONFIG.TDATA_NUM_BYTES [get_bd_intf_pins cnn_core/out_r]]

puts "WIDTHS_DETECTED:"
puts "LISTA H_OUT WIDTH: $lista_w bytes"
puts "CNN REFINE_INPUT WIDTH: $cnn_w_in bytes"
puts "CNN RESIDUAL_BASE WIDTH: $cnn_w_res bytes"
puts "CNN OUT_R WIDTH: $cnn_w_out bytes"
