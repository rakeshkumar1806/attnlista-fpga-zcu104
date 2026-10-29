setws vitis_workspace
set apps [repo -apps]
foreach line [split $apps "\n"] {
    if {[string match -nocase "*C++*" $line]} {
        puts $line
    }
}
