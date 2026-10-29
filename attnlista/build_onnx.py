import torch
import hls4ml
import os
from refinement_cnn_standalone import RefinementCNN, load_trained_weights
import onnx

N_BS, N_PATHS = 16, 65
model = RefinementCNN(n_bs=N_BS, n_paths=N_PATHS)
model = load_trained_weights(model, "hls_export")
model.eval()

dummy_input = torch.randn(1, 2*N_BS + 32, N_PATHS)
torch.onnx.export(model, dummy_input, "refinement_cnn.onnx", input_names=['input'], output_names=['output'])

# Now load and convert with ONNX
onnx_model = onnx.load("refinement_cnn.onnx")

config = hls4ml.utils.config_from_onnx_model(onnx_model)
config['Model']['Precision'] = 'ap_fixed<16,6>'
config['Model']['ReuseFactor'] = 1
config['Model']['Strategy'] = 'Resource'
config['OutputDir'] = 'hls4ml_prj_onnx'
config['ProjectName'] = 'myproject'
config['Part'] = 'xczu7ev-ffvc1156-2-e'

hls_model = hls4ml.converters.convert_from_onnx_model(
    onnx_model,
    hls_config=config,
)

hls_model.compile()
print("Starting HLS synthesis for ONNX model...")
hls_model.build(csim=False, synth=True, vsynth=False, export=False)
try:
    report = hls4ml.report.read_vivado_report('hls4ml_prj_onnx')
    print("Synthesis Report:")
    print(report)
except Exception as e:
    print("Failed to read report:", e)
