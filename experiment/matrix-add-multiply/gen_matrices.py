# todo: reimplement this in python:

# # include <cstddef> // For size_t
# # include <cstdint>
# # include <iostream>

# uint32_t hash(uint32_t x) {
#   // sdbm
#   size_t rv = 0;
#   for (int i = 0; i < 4; i++) {
#     rv = (x & 0xFF) + (rv << 6) + (rv << 16) - rv;
#     x = x >> 8;
#   }

#   return rv;
# }

# // needs to be recursive because of reasons

# int main() {

#   uint32_t h = hash(1);
#   for (int j = 0; j < 3; j++) {
#     for (int i = 0; i < 1024 * 1024; i++) {
#       float x = ((float)h / (float)UINT32_MAX) * 2.0 - 1.0;
#       printf("%f, ", x);
#       h = hash(h);
#     }
#     printf("\n");
#   }
# }