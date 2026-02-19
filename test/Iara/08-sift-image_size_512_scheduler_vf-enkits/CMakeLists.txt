# Build configuration from experiments.yaml
set(EXTRA_KERNEL_ARGS -DVERBOSE CACHE STRING "Extra kernel compilation arguments")
set(EXTRA_LINKER_ARGS -lm CACHE STRING "Extra linker arguments")

# Export parameters as environment variables for codegen.sh
set(ENV{IMAGE_SIZE} 512)
set(ENV{SCHEDULER} vf-enkits)
