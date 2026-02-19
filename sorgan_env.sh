IARA_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

module load casacore cmake ccache cuda gdb/15.2 openblas python/3.13 mold gcc/14.2 abseil-cpp valgrind

# Override global spack (v0.24.0 from /etc/profile.d) with local version
source "$IARA_DIR/spack/share/spack/setup-env.sh"
spack env activate iara_env

for i in gdb gcc ninja cmake ccache casacore abseil-cpp mold valgrind python/3.13.1 openblas; do
  module load $i
done

export NVM_DIR="$HOME/.nvm"
[ -s "$NVM_DIR/nvm.sh" ] && source "$NVM_DIR/nvm.sh"
nvm use v25.6.1 > /dev/null 2>&1

VENV_DIR="$IARA_DIR/.venv"

if [ ! -d "$VENV_DIR" ]; then
  echo "Creating Python virtual environment in $VENV_DIR..."
  python3 -m venv "$VENV_DIR"
  source "$VENV_DIR/bin/activate"
  pip install --upgrade pip
  pip install -r "$IARA_DIR/requirements.txt"
else
  source "$VENV_DIR/bin/activate"
fi

source "$IARA_DIR/.env"
