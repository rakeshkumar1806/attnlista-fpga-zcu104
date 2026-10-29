$logFile = "C:\Users\jbvrr\.gemini\antigravity\brain\aeeb61f1-2bf1-4a74-951d-9538e3a22362\.system_generated\tasks\task-3228.log"
while ($true) {
    if (Test-Path $logFile) {
        $content = Get-Content $logFile
        if ($content -match "VITIS PROJECT CREATED") {
            Write-Host "Project created!"
            break
        }
    }
    Start-Sleep -Seconds 5
}
Write-Host "Copying sources..."
Copy-Item main.cpp vitis_workspace\attnlista_app\src\ -Force
Copy-Item test_vectors.h vitis_workspace\attnlista_app\src\ -Force
Write-Host "Building application..."
C:\Xilinx\Vitis\2022.2\bin\xsct.bat build_vitis.tcl
Write-Host "DONE"
