Stage0 += baseimage(image='ubuntu:22.04')

Stage0 += apt_get(ospackages=['gcc', 'g++', 'gfortran', 'cmake', 'ninja-build', 'mold', 'lldb', 'gdb', 'ccache'])

