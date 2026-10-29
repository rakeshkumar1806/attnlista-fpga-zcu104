import torch
import hls4ml
from refinement_cnn_standalone import RefinementCNN

N_BS, N_PATHS = 16, 65
model = RefinementCNN(n_bs=N_BS, n_paths=N_PATHS)
input_shape = (1, 64, 65)

config = hls4ml.utils.config_from_pytorch_model(model, input_shape)
config['Model']['Precision'] = 'ap_fixed<16,6>'
config['Model']['ReuseFactor'] = 1

config['OutputDir'] = 'hls4ml_prj_test'
config['Part'] = 'xczu7ev-ffvc1156-2-e'
hls_model = hls4ml.converters.convert_from_pytorch_model(
    model, 
    input_shape,
    hls_config=config
)
print("Config IOType:", config.get("IOType"))
print("Model config:", hls_model.config.config)
