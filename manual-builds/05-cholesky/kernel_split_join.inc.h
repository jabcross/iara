void kernel_split(double *out_0_0, double *out_1_0, double *out_2_0, double *out_3_0, double *out_0_1, double *out_1_1, double *out_2_1, double *out_3_1, double *out_0_2, double *out_1_2, double *out_2_2, double *out_3_2, double *out_0_3, double *out_1_3, double *out_2_3, double *out_3_3) {
memcpy(out_0_0, &input_g[0 * block_size_doubles], block_size_bytes);
memcpy(out_0_1, &input_g[1 * block_size_doubles], block_size_bytes);
memcpy(out_0_2, &input_g[2 * block_size_doubles], block_size_bytes);
memcpy(out_0_3, &input_g[3 * block_size_doubles], block_size_bytes);
memcpy(out_1_0, &input_g[4 * block_size_doubles], block_size_bytes);
memcpy(out_1_1, &input_g[5 * block_size_doubles], block_size_bytes);
memcpy(out_1_2, &input_g[6 * block_size_doubles], block_size_bytes);
memcpy(out_1_3, &input_g[7 * block_size_doubles], block_size_bytes);
memcpy(out_2_0, &input_g[8 * block_size_doubles], block_size_bytes);
memcpy(out_2_1, &input_g[9 * block_size_doubles], block_size_bytes);
memcpy(out_2_2, &input_g[10 * block_size_doubles], block_size_bytes);
memcpy(out_2_3, &input_g[11 * block_size_doubles], block_size_bytes);
memcpy(out_3_0, &input_g[12 * block_size_doubles], block_size_bytes);
memcpy(out_3_1, &input_g[13 * block_size_doubles], block_size_bytes);
memcpy(out_3_2, &input_g[14 * block_size_doubles], block_size_bytes);
memcpy(out_3_3, &input_g[15 * block_size_doubles], block_size_bytes);
}
void kernel_join(double *in_0_0, double *in_1_0, double *in_2_0, double *in_3_0, double *in_0_1, double *in_1_1, double *in_2_1, double *in_3_1, double *in_0_2, double *in_1_2, double *in_2_2, double *in_3_2, double *in_0_3, double *in_1_3, double *in_2_3, double *in_3_3) {
memcpy(&output_g[0 * block_size_doubles], in_0_0, block_size_bytes);
memcpy(&output_g[1 * block_size_doubles], in_0_1, block_size_bytes);
memcpy(&output_g[2 * block_size_doubles], in_0_2, block_size_bytes);
memcpy(&output_g[3 * block_size_doubles], in_0_3, block_size_bytes);
memcpy(&output_g[4 * block_size_doubles], in_1_0, block_size_bytes);
memcpy(&output_g[5 * block_size_doubles], in_1_1, block_size_bytes);
memcpy(&output_g[6 * block_size_doubles], in_1_2, block_size_bytes);
memcpy(&output_g[7 * block_size_doubles], in_1_3, block_size_bytes);
memcpy(&output_g[8 * block_size_doubles], in_2_0, block_size_bytes);
memcpy(&output_g[9 * block_size_doubles], in_2_1, block_size_bytes);
memcpy(&output_g[10 * block_size_doubles], in_2_2, block_size_bytes);
memcpy(&output_g[11 * block_size_doubles], in_2_3, block_size_bytes);
memcpy(&output_g[12 * block_size_doubles], in_3_0, block_size_bytes);
memcpy(&output_g[13 * block_size_doubles], in_3_1, block_size_bytes);
memcpy(&output_g[14 * block_size_doubles], in_3_2, block_size_bytes);
memcpy(&output_g[15 * block_size_doubles], in_3_3, block_size_bytes);
}
