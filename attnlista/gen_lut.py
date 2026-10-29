import math
# Generate 256-entry sigmoid LUT covering [-8, +8]
# Each entry: round(65535 * sigmoid(-8 + i * 16/256))
vals = []
for i in range(256):
    x = -8.0 + i * 16.0 / 256.0
    s = 1.0 / (1.0 + math.exp(-x))
    vals.append(int(round(s * 65535)))

# Print as C array
print("static const unsigned short sigmoid_lut[256] = {")
for i in range(0, 256, 8):
    chunk = vals[i:i+8]
    print("    " + ", ".join(str(v) for v in chunk) + ",")
print("};")
