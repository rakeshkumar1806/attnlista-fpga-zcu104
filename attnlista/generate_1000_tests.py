import numpy as np
NUM_TESTS = 1000
y = np.load('C:/Users/jbvrr/Downloads/deepmimo_tb_y_input.npy').astype(np.float32)
y = y[0::2] + 1j * y[1::2]
y = y.reshape(NUM_TESTS, 16, 16)
h_true = np.load('C:/Users/jbvrr/Downloads/deepmimo_tb_h_true_expected.npy').astype(np.float32)
h_true = h_true[0::2] + 1j * h_true[1::2]
h_true = h_true.reshape(NUM_TESTS, 16, 65)

with open('vitis_workspace/attnlista_app/src/deepmimo_test_vectors_multi.h', 'w') as f:
    f.write('const float tb_y_input[] = {\n')
    for t in range(NUM_TESTS):
        # y_input hardware layout: real, imag, real, imag... for each row/col.
        # Just flatten the complex array by creating [real, imag] pairs
        y_flat = np.zeros(512, dtype=np.float32)
        y_flat[0::2] = y[t].real.flatten()
        y_flat[1::2] = y[t].imag.flatten()
        f.write(', '.join([f'{val}f' for val in y_flat]))
        f.write(',\n')
    f.write('};\n\n')
    f.write('const float tb_h_true_expected[] = {\n')
    for t in range(NUM_TESTS):
        x_ri = np.concatenate([h_true[t].real, h_true[t].imag], axis=0)
        flat_expected = x_ri.T.flatten()
        f.write(', '.join([f'{val}f' for val in flat_expected]))
        f.write(',\n')
    f.write('};\n')
print('Successfully generated correct header')
