\* Buffer_Size *\
Minimize
Size_of_buffer_with_delays: 3 alpha3
Subject To
alphas_0: 3 alpha0 - alpha1 = -7
alphas_1: alpha1 - 2 alpha2 = 0
alphas_2: 2 alpha2 - 3 alpha3 = 0
betas_0: 3 beta0 - beta1 = 0
betas_1: beta1 - 2 beta2 = 0
betas_2: 2 beta2 - 3 beta3 = 0
remove_trivial_case_of_alpha_1_=_0: alpha1 >= 1
remove_trivial_case_of_alpha_2_=_0: alpha2 >= 1
remove_trivial_case_of_alpha_3_=_0: alpha3 >= 1
remove_trivial_case_of_beta_1_=_0: beta1 >= 1
remove_trivial_case_of_beta_2_=_0: beta2 >= 1
remove_trivial_case_of_beta_3_=_0: beta3 >= 1
Bounds
 0 <= alpha0
 0 <= alpha1
 0 <= alpha2
 0 <= alpha3
 0 <= beta0
 0 <= beta1
 0 <= beta2
 0 <= beta3
Generals
alpha0
alpha1
alpha2
alpha3
beta0
beta1
beta2
beta3
End
