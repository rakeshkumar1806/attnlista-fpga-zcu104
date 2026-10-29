setws vitis_workspace
# If the app already exists (or half exists), we can just build it or recreate it.
# Let's delete the old broken app if it exists.
catch {app remove attnlista_app}
app create -name attnlista_app -platform hw_plat -domain standalone_domain -template "Empty Application(C)"
puts "VITIS PROJECT CREATED"
