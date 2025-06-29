\* Buffer_Size *\
Minimize
Size_of_buffer_with_delays: 4194304 alpha1
Subject To
alphas_0: 4 alpha0 - 4194304 alpha1 = 0
betas_0: 4 beta0 - 4194304 beta1 = 0
remove_trivial_case_of_alpha_1_=_0: alpha1 >= 1
remove_trivial_case_of_beta_1_=_0: beta1 >= 1
Bounds
 0 <= alpha0
 0 <= alpha1
 0 <= beta0
 0 <= beta1
Generals
alpha0
alpha1
beta0
beta1
End
