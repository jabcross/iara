import sys


if __name__ == "__main__":
    assert (len(sys.argv) == 3)
    matrix_size = int(sys.argv[1])
    num_blocks = int(sys.argv[2])
    ts = matrix_size // num_blocks

    print(f"void kernel_split({", ".join([f"double *out_{i}_{j}" for j in range(num_blocks)
                                          for i in range(num_blocks)])}) {{")
    for i in range(num_blocks):
        for j in range(num_blocks):
            block_num = i * num_blocks + j
            print(
                f"memcpy(out_{i}_{j}, &input_g[{block_num} * block_size_doubles], block_size_bytes);")
    print("}")

    print(f"void kernel_join({", ".join([f"double *in_{i}_{j}" for j in range(num_blocks)
                                         for i in range(num_blocks)])}) {{")
    for i in range(num_blocks):
        for j in range(num_blocks):
            block_num = i * num_blocks + j
            print(
                f"memcpy(&output_g[{block_num} * block_size_doubles], in_{i}_{j}, block_size_bytes);")
    print("}")
