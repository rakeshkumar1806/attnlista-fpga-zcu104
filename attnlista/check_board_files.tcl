set boards [get_board_parts *zcu104*]
if {[llength $boards] > 0} {
    puts "FOUND_BOARDS: $boards"
} else {
    puts "FOUND_BOARDS: NONE"
}
