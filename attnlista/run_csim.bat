@echo off
echo =========================================================
echo Compiling AttnLISTA C++ Bit-Accurate Simulation...
echo =========================================================
g++ -O3 -I"C:\Xilinx\Vitis_HLS\2022.2\include" fast_sim.cpp attnlista_top.cpp cnn_hls_core.cpp -o fast_sim.exe

if %ERRORLEVEL% GEQ 1 (
    echo [ERROR] Compilation failed.
    exit /b
)

echo [SUCCESS] Compilation finished. Running HLS C-Simulation...
echo.
fast_sim.exe
