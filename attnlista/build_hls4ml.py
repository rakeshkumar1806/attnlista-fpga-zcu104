import torch
import hls4ml
import os
from refinement_cnn_standalone import RefinementCNN, load_trained_weights

N_BS, N_PATHS = 16, 65
model = RefinementCNN(n_bs=N_BS, n_paths=N_PATHS)
model = load_trained_weights(model, "hls_export")
model.eval()

input_shape = (1, 64, 65)

# Convert to hls4ml
config = hls4ml.utils.config_from_pytorch_model(model, input_shape)
config['Model']['Precision'] = 'ap_fixed<16,6>'
config['Model']['ReuseFactor'] = 1
# Strategy Resource since it's the project goal
config['Model']['Strategy'] = 'Resource'
config['Model']['IOType'] = 'io_parallel'

config['OutputDir'] = 'hls4ml_prj'
config['ProjectName'] = 'myproject'
config['Part'] = 'xczu7ev-ffvc1156-2-e'
hls_model = hls4ml.converters.convert_from_pytorch_model(
    model, 
    input_shape,
    hls_config=config, 
)

hls_model.compile()
print("Starting HLS synthesis for RefinementCNN...")
# Run synthesis
hls_model.build(csim=False, synth=True, vsynth=False, export=False)
try:
    report = hls4ml.report.read_vivado_report('hls4ml_prj')
    print("Synthesis Report:")
    print(report)
except Exception as e:
    print("Failed to read report:", e)
