 = Get-Content attnlista_hls_core.h -Raw
 =  -replace 'const pmat_t P_mat_H\[N_PATHS\]\[M_MEAS\]', 'const pmat_t P_mat_H[M_MEAS][N_PATHS]'
 =  -replace 'P_mat\[m\]\[p\]', 'P_mat[p][m]'
 =  -replace 'P_mat_H\[p\]\[m\]', 'P_mat_H[m][p]'
Set-Content -Path attnlista_hls_core.h -Value 
