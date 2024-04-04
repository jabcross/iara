dif main {

actortype kernel_potrf {
              
    consumption in_A: f64 = 262144;
    production out_A: f64 = 262144;

}


actortype kernel_trsm {
              
    consumption in_A: f64 = 262144;
    consumption in_B: f64 = 262144;
    production out_B: f64 = 262144;

}


actortype kernel_gemm {
              
    consumption in_A: f64 = 262144;
    consumption in_B: f64 = 262144;
    consumption in_C: f64 = 262144;
    production out_C: f64 = 262144;

}


actortype kernel_syrk {
              
    consumption in_A: f64 = 262144;
    consumption in_B: f64 = 262144;
    production out_B: f64 = 262144;

}


actortype kernel_split {

    production out_0_0 : f64 = 262144;
    production out_0_1 : f64 = 262144;
    production out_0_2 : f64 = 262144;
    production out_0_3 : f64 = 262144;
    production out_1_0 : f64 = 262144;
    production out_1_1 : f64 = 262144;
    production out_1_2 : f64 = 262144;
    production out_1_3 : f64 = 262144;
    production out_2_0 : f64 = 262144;
    production out_2_1 : f64 = 262144;
    production out_2_2 : f64 = 262144;
    production out_2_3 : f64 = 262144;
    production out_3_0 : f64 = 262144;
    production out_3_1 : f64 = 262144;
    production out_3_2 : f64 = 262144;
    production out_3_3 : f64 = 262144;

}

actor split_0 {
    type: kernel_split;

    interface out_0_0->e_0_0_0;
    interface out_0_1->e_0_1_0;
    interface out_0_2->e_0_2_0;
    interface out_0_3->e_0_3_0;
    interface out_1_0->e_1_0_0;
    interface out_1_1->e_1_1_0;
    interface out_1_2->e_1_2_0;
    interface out_1_3->e_1_3_0;
    interface out_2_0->e_2_0_0;
    interface out_2_1->e_2_1_0;
    interface out_2_2->e_2_2_0;
    interface out_2_3->e_2_3_0;
    interface out_3_0->e_3_0_0;
    interface out_3_1->e_3_1_0;
    interface out_3_2->e_3_2_0;
    interface out_3_3->e_3_3_0;

}

actor n000 {
    type: kernel_potrf;
    interface e_0_0_0->in_A;
    interface out_A->e_0_0_1;
}
actor n001 {
    type: kernel_potrf;
    interface e_1_1_0->in_A;
    interface out_A->e_1_1_1;
}
actor n002 {
    type: kernel_potrf;
    interface e_2_2_0->in_A;
    interface out_A->e_2_2_1;
}
actor n003 {
    type: kernel_potrf;
    interface e_3_3_0->in_A;
    interface out_A->e_3_3_1;
}
actor n004 {
    type: kernel_trsm;
    interface e_0_0_1->in_A;
    interface e_0_1_0->in_B;
    interface out_B->e_0_1_1;
}
actor n005 {
    type: kernel_trsm;
    interface e_0_0_1->in_A;
    interface e_0_2_0->in_B;
    interface out_B->e_0_2_1;
}
actor n006 {
    type: kernel_trsm;
    interface e_0_0_1->in_A;
    interface e_0_3_0->in_B;
    interface out_B->e_0_3_1;
}
actor n007 {
    type: kernel_trsm;
    interface e_1_1_1->in_A;
    interface e_1_2_0->in_B;
    interface out_B->e_1_2_1;
}
actor n008 {
    type: kernel_trsm;
    interface e_1_1_1->in_A;
    interface e_1_3_0->in_B;
    interface out_B->e_1_3_1;
}
actor n009 {
    type: kernel_trsm;
    interface e_2_2_1->in_A;
    interface e_2_3_0->in_B;
    interface out_B->e_2_3_1;
}
actor n010 {
    type: kernel_syrk;
    interface e_0_1_1->in_A;
    interface e_1_1_1->in_B;
    interface out_B->e_1_1_2;
}
actor n011 {
    type: kernel_gemm;
    interface e_0_2_1->in_A;
    interface e_0_1_1->in_B;
    interface e_1_2_1->in_C;
    interface out_C->e_1_2_2;
}
actor n012 {
    type: kernel_syrk;
    interface e_0_2_1->in_A;
    interface e_2_2_1->in_B;
    interface out_B->e_2_2_2;
}
actor n013 {
    type: kernel_gemm;
    interface e_0_3_1->in_A;
    interface e_0_1_1->in_B;
    interface e_1_3_1->in_C;
    interface out_C->e_1_3_2;
}
actor n014 {
    type: kernel_gemm;
    interface e_0_3_1->in_A;
    interface e_0_2_1->in_B;
    interface e_2_3_1->in_C;
    interface out_C->e_2_3_2;
}
actor n015 {
    type: kernel_syrk;
    interface e_0_3_1->in_A;
    interface e_3_3_1->in_B;
    interface out_B->e_3_3_2;
}
actor n016 {
    type: kernel_syrk;
    interface e_1_2_2->in_A;
    interface e_2_2_2->in_B;
    interface out_B->e_2_2_3;
}
actor n017 {
    type: kernel_gemm;
    interface e_1_3_2->in_A;
    interface e_1_2_2->in_B;
    interface e_2_3_2->in_C;
    interface out_C->e_2_3_3;
}
actor n018 {
    type: kernel_syrk;
    interface e_1_3_2->in_A;
    interface e_3_3_2->in_B;
    interface out_B->e_3_3_3;
}
actor n019 {
    type: kernel_syrk;
    interface e_2_3_3->in_A;
    interface e_3_3_3->in_B;
    interface out_B->e_3_3_4;
}

actortype kernel_join {

    consumption in_0_0 : f64 = 262144;
    consumption in_0_1 : f64 = 262144;
    consumption in_0_2 : f64 = 262144;
    consumption in_0_3 : f64 = 262144;
    consumption in_1_0 : f64 = 262144;
    consumption in_1_1 : f64 = 262144;
    consumption in_1_2 : f64 = 262144;
    consumption in_1_3 : f64 = 262144;
    consumption in_2_0 : f64 = 262144;
    consumption in_2_1 : f64 = 262144;
    consumption in_2_2 : f64 = 262144;
    consumption in_2_3 : f64 = 262144;
    consumption in_3_0 : f64 = 262144;
    consumption in_3_1 : f64 = 262144;
    consumption in_3_2 : f64 = 262144;
    consumption in_3_3 : f64 = 262144;

}

actor join_0 {
    type: kernel_join;

    interface e_0_0_1->in_0_0;
    interface e_0_1_1->in_0_1;
    interface e_0_2_1->in_0_2;
    interface e_0_3_1->in_0_3;
    interface e_1_0_0->in_1_0;
    interface e_1_1_2->in_1_1;
    interface e_1_2_2->in_1_2;
    interface e_1_3_2->in_1_3;
    interface e_2_0_0->in_2_0;
    interface e_2_1_0->in_2_1;
    interface e_2_2_3->in_2_2;
    interface e_2_3_3->in_2_3;
    interface e_3_0_0->in_3_0;
    interface e_3_1_0->in_3_1;
    interface e_3_2_0->in_3_2;
    interface e_3_3_4->in_3_3;

}

}
