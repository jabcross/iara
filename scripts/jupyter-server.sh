#!/usr/bin/env bash
# Launch, relaunch, stop, or report the Jupyter Lab server.
#
# The server runs detached (setsid) and writes its PID to a pidfile, so this
# script can control it across shell sessions.
#
# Runtime files are stored on local disk (/scratch) because the NFS home dir
# does not honor 0o600 file modes, which breaks jupyter_core secure_write.
#
# Usage:
#   scripts/jupyter-server.sh start
#   scripts/jupyter-server.sh relaunch
#   scripts/jupyter-server.sh stop
#   scripts/jupyter-server.sh status
#
# Env overrides: JUPYTER_STATE_DIR, JUPYTER_PORT, JUPYTER_BIN

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

JUPYTER_STATE_DIR="${JUPYTER_STATE_DIR:-${HOME}/scratch/.jupyter}"
JUPYTER_RUNTIME_DIR="${JUPYTER_STATE_DIR}/runtime"
LOG_FILE="${JUPYTER_STATE_DIR}/jupyter.log"
PID_FILE="${JUPYTER_STATE_DIR}/jupyter.pid"
PORT="${JUPYTER_PORT:-8888}"
JUPYTER_BIN="${JUPYTER_BIN:-jupyter}"

usage() {
  sed -n '2,20p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
}

pid() {
  cat "${PID_FILE}" 2>/dev/null || true
}

is_running() {
  local p
  p="$(pid)"
  [[ -n "${p}" ]] && kill -0 "${p}" 2>/dev/null
}

wait_for_server() {
  local i
  for i in $(seq 1 30); do
    if curl -s -o /dev/null "http://localhost:${PORT}/lab" 2>/dev/null; then
      return 0
    fi
    sleep 1
  done
  return 1
}

show_url() {
  local token
  token="$(sed -n 's/.*token=\([0-9a-fA-F]*\).*/\1/p' "${LOG_FILE}" 2>/dev/null | tail -1)"
  echo "Jupyter Lab: http://localhost:${PORT}/lab?token=${token}"
}

start() {
  if is_running; then
    echo "Already running (pid $(pid)). Use 'relaunch' or 'stop' first." >&2
    return 1
  fi
  mkdir -p "${JUPYTER_RUNTIME_DIR}"
  setsid env JUPYTER_RUNTIME_DIR="${JUPYTER_RUNTIME_DIR}" "${JUPYTER_BIN}" \
    lab --no-browser --port "${PORT}" > "${LOG_FILE}" 2>&1 < /dev/null &
  echo $! > "${PID_FILE}"
  if wait_for_server; then
    show_url
  else
    echo "Server did not become ready. Check ${LOG_FILE}" >&2
    return 1
  fi
}

stop() {
  local p
  p="$(pid)"
  if [[ -z "${p}" ]] || ! kill -0 "${p}" 2>/dev/null; then
    rm -f "${PID_FILE}"
    echo "Not running."
    return 0
  fi
  kill "${p}"
  for i in $(seq 1 10); do
    kill -0 "${p}" 2>/dev/null || break
    sleep 0.5
  done
  rm -f "${PID_FILE}"
  echo "Stopped (pid ${p})."
}

case "${1:-}" in
  start)    start ;;
  relaunch) stop; start ;;
  stop)     stop ;;
  status)
    if is_running; then
      echo "Running (pid $(pid), port ${PORT})."
      show_url
    else
      echo "Not running."
    fi
    ;;
  *) usage ;;
esac
