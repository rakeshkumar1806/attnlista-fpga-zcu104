import os

repo = r'C:\Users\jbvrr\.gemini\antigravity\scratch\attnlista'
report_file = r'C:\Users\jbvrr\.gemini\antigravity\brain\aeeb61f1-2bf1-4a74-951d-9538e3a22362\attnlista_final_report.md'
out_file = os.path.join(repo, 'consolidated_attnlista_package.md')

def get_content(filename):
    path = os.path.join(repo, filename)
    if os.path.exists(path):
        with open(path, 'r', encoding='utf-8') as f:
            return f.read()
    elif os.path.exists(filename):
        with open(filename, 'r', encoding='utf-8') as f:
            return f.read()
    return f'// {filename} not found.'

def get_rtl_snippet(filename):
    path = os.path.join(repo, filename)
    if not os.path.exists(path):
        return f'// RTL {filename} not found.'
    
    with open(path, 'r', encoding='utf-8') as f:
        return f.read()

with open(out_file, 'w', encoding='utf-8') as out:
    out.write('# AttnLISTA RIS Channel Estimator - Consolidated Hardware Flow\n\n')
    out.write('This document consolidates the C++ source code, testbenches, TCL scripts, RTL interfaces, and final verified results for the AttnLISTA RIS Channel Estimator mapped to the ZCU104 FPGA.\n\n')
    
    out.write('## 1. Final Results and Report\n')
    out.write(get_content(report_file) + '\n\n')
    
    out.write('## 2. Core C++ Implementation\n')
    
    out.write('### `attnlista_hls_core.h` (LISTA Core logic)\n')
    out.write('```cpp\n' + get_content('attnlista_hls_core.h') + '\n```\n\n')
    
    out.write('### `cnn_hls_core.h` (Refinement CNN logic)\n')
    out.write('```cpp\n' + get_content('cnn_hls_core.h') + '\n```\n\n')
    
    out.write('### `cnn_hls_core.cpp` (Refinement CNN implementation)\n')
    out.write('```cpp\n' + get_content('cnn_hls_core.cpp') + '\n```\n\n')
    
    out.write('### `attnlista_top.cpp` (Top-level Wrapper & FSM)\n')
    out.write('```cpp\n' + get_content('attnlista_top.cpp') + '\n```\n\n')
    
    out.write('## 3. Testbench\n')
    out.write('### `attnlista_tb.cpp` (LISTA Testbench)\n')
    out.write('```cpp\n' + get_content('attnlista_tb.cpp') + '\n```\n\n')
    
    out.write('### `cnn_tb.cpp` (CNN Testbench)\n')
    out.write('```cpp\n' + get_content('cnn_tb.cpp') + '\n```\n\n')
    
    out.write('## 4. Run Scripts\n')
    out.write('### `run_lista_synth_unrolled.tcl` (LISTA Flow)\n')
    out.write('```tcl\n' + get_content('run_lista_synth_unrolled.tcl') + '\n```\n\n')
    
    out.write('### `run_cnn.tcl` (CNN Flow)\n')
    out.write('```tcl\n' + get_content('run_cnn.tcl') + '\n```\n\n')
    
    out.write('## 5. RTL Wrapper Interface\n')
    out.write('### `attnlista_top.v` (Interface snippet)\n')
    out.write('```verilog\n' + get_rtl_snippet('lista_prj_synth\\solution1\\syn\\verilog\\attnlista_top.v') + '\n```\n')

print(f'Wrote to {out_file}')
