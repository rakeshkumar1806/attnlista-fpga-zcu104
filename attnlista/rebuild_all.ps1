Write-Host "Running HLS Synthesis..."
C:\Xilinx\Vitis_HLS\2022.2\bin\vitis_hls.bat -f run_synth_only.tcl
C:\Xilinx\Vitis_HLS\2022.2\bin\vitis_hls.bat -f export_lista.tcl
Write-Host "Rebuilding Vivado..."
C:\Xilinx\Vivado\2022.2\bin\vivado.bat -mode batch -source rebuild_vivado.tcl
Write-Host "DONE REBUILDING EVERYTHING"
