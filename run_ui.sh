#!/bin/bash
# Launch iara experiment DB UI. Laptop: ssh -L 8501:localhost:8501 sorgan
set -e
cd "$(dirname "$0")"
PY=/scratch/pedro.ciambra/repos/iara/.venv/bin/python
# refresh DB if new results exist (ingest is incremental + fast)
$PY ingest.py 2>/dev/null | tail -1 || true
exec $PY -m streamlit run ui/app.py --server.port "${PORT:-8501}" "$@"
