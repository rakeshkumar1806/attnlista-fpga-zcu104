import os

repo = r'C:\Users\jbvrr\.gemini\antigravity\scratch\attnlista'
out_file = r'C:\Users\jbvrr\.gemini\antigravity\brain\aeeb61f1-2bf1-4a74-951d-9538e3a22362\cnn_components.md'

def get_content(filename):
    path = os.path.join(repo, filename)
    if os.path.exists(path):
        with open(path, 'r', encoding='utf-8') as f:
            return f.read()
    return f'// {filename} not found.'

with open(out_file, 'w', encoding='utf-8') as out:
    out.write('# Refinement CNN Components\n\n')
    
    out.write('## `cnn_hls_core.h`\n')
    out.write('```cpp\n' + get_content('cnn_hls_core.h') + '\n```\n\n')
    
    out.write('## `cnn_hls_core.cpp`\n')
    out.write('```cpp\n' + get_content('cnn_hls_core.cpp') + '\n```\n\n')
    
    out.write('## `cnn_tb.cpp`\n')
    out.write('```cpp\n' + get_content('cnn_tb.cpp') + '\n```\n\n')
    
    out.write('## `run_cnn.tcl`\n')
    out.write('```tcl\n' + get_content('run_cnn.tcl') + '\n```\n\n')

print(f'Wrote to {out_file}')
