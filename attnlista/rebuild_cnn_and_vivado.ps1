$ErrorActionPreference = "Stop"

Write-Host "--- 1. Running Vitis HLS for CNN ---"
$hls = Start-Process -FilePath "C:\Xilinx\Vitis_HLS\2022.2\bin\vitis_hls.bat" -ArgumentList "-f run_cnn_synth.tcl" -Wait -NoNewWindow -PassThru
if ($hls.ExitCode -ne 0) {
    Write-Error "HLS failed!"
    exit 1
}

Write-Host "--- 2. Rebuilding Vivado Bitstream ---"
$vivado = Start-Process -FilePath "C:\Xilinx\Vivado\2022.2\bin\vivado.bat" -ArgumentList "-mode batch -source rebuild_vivado.tcl" -Wait -NoNewWindow -PassThru
if ($vivado.ExitCode -ne 0) {
    Write-Error "Vivado failed!"
    exit 1
}

Write-Host "SUCCESS! The new bitstream is ready."
