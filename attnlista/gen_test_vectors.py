import numpy as np

def to_c_array(filename, array_name, is_complex=True):
    with open(filename, 'r') as f:
        lines = f.readlines()
    
    c_str = f"const float {array_name}[] = {{\n"
    vals = []
    for line in lines:
        if not line.strip(): continue
        if is_complex:
            # Assuming format "real imag" or "real,imag"
            parts = line.replace(',', ' ').split()
            if len(parts) >= 2:
                vals.append(f"{parts[0]}f, {parts[1]}f")
            else:
                c = complex(line.strip().replace('i', 'j'))
                vals.append(f"{c.real}f, {c.imag}f")
        else:
            vals.append(f"{float(line.strip())}f")
    
    c_str += ", ".join(vals)
    c_str += "\n};\n"
    return c_str

with open("test_vectors.h", "w") as out:
    out.write("#pragma once\n\n")
    out.write(to_c_array("tb_y_input.txt", "tb_y_input", True))
    out.write(to_c_array("tb_cnn_expected.txt", "tb_cnn_expected", True))
    out.write(to_c_array("tb_h_lista_expected.txt", "tb_lista_expected", True))
print("Generated test_vectors.h")
