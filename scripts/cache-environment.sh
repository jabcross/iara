#!/usr/bin/env bash
# Snapshot the current shell environment (variables and functions) into .env.cached.
# Usage: source "${BASH_SOURCE[0]}" from the shell you want to capture.

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
  printf 'Please source this script instead of executing it directly.\n' >&2
  exit 1
fi

cache_env() {
  local script_dir repo_root outfile tmpfile timestamp
  script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  repo_root="$(cd "${script_dir}/.." && pwd)"
  outfile="${repo_root}/.env.cached"
  tmpfile="$(mktemp "${TMPDIR:-/tmp}/env-cache.XXXXXX")" || {
    printf 'Failed to create temporary file.\n' >&2
    return 1
  }

  local -a skip_vars=(
    BASH BASHOPTS BASH_ALIASES BASH_ARGC BASH_ARGV BASH_CMDS BASH_EXECUTION_STRING
    BASH_LINENO BASH_SOURCE BASH_SUBSHELL BASH_VERSINFO BASHPID DIRSTACK EPOCHREALTIME
    EPOCHSECONDS FUNCNAME GROUPS PIPESTATUS SHELLOPTS BASH_COMMAND BASH_XTRACEFD
    PWD OLDPWD SHLVL _ RANDOM SECONDS LINENO PPID UID EUID TMPDIR HOSTNAME
    HISTCMD HISTFILE HISTFILESIZE HISTSIZE MAILCHECK OPTARG OPTIND REPLY
    SPACK_ENV SPACK_ENV_VIEW SPACK_ROOT
    PROMPT_COMMAND PS0 PS1 PS2 PS3 PS4
    VSCODE_SHELL_INTEGRATION VSCODE_INJECTION VSCODE_GIT_IPC_HANDLE
    VSCODE_GIT_ASKPASS_MAIN VSCODE_GIT_ASKPASS_NODE VSCODE_GIT_ASKPASS_EXTRA_ARGS
    VSCODE_IPC_HOOK_CLI VSCODE_PYTHON_AUTOACTIVATE_GUARD VSCODE_DEBUGPY_ADAPTER_ENDPOINTS
    GEMINI_CLI GEMINI_API_KEY GEMINI_CLI_IDE_AUTH_TOKEN
    GEMINI_CLI_IDE_SERVER_PORT GEMINI_CLI_IDE_WORKSPACE_PATH
    CLAUDECODE CLAUDE_CODE_SSE_PORT
    SSH_CLIENT SSH_CONNECTION SSH_TTY SSH_ASKPASS
    DBUS_SESSION_BUS_ADDRESS
    XDG_RUNTIME_DIR XDG_SESSION_ID XDG_SESSION_TYPE XDG_SESSION_CLASS
    GIT_ASKPASS
    TERM_PROGRAM TERM_PROGRAM_VERSION COLORTERM BROWSER
    STARSHIP_KEY STARSHIP_SESSION_KEY STARSHIP_SHELL STARSHIP_PROMPT_COMMAND
    APPTAINER_CACHEDIR APPTAINER_TMPDIR
    PYDEVD_DISABLE_FILE_VALIDATION BUNDLED_DEBUGPY_PATH
    script_dir repo_root outfile tmpfile timestamp skip_vars skip_lookup skip_funcs
    skip_func_lookup key fn var
  )
  local -A skip_lookup=()
  local key
  for key in "${skip_vars[@]}"; do
    skip_lookup["${key}"]=1
  done

  local -a skip_funcs=(cache_env)
  local -A skip_func_lookup=()
  for key in "${skip_funcs[@]}"; do
    skip_func_lookup["${key}"]=1
  done

  timestamp="$(date -Iseconds 2>/dev/null || date)"

  {
    printf '#!/usr/bin/env bash\n'
    printf '# Environment snapshot generated on %s\n' "${timestamp}"
    printf '# Source this file to restore the saved session state.\n'
    printf '\n'

    printf '# Functions\n'
    local line fn
    while IFS= read -r line; do
      fn="${line##* }"
      if [[ -n "${skip_func_lookup[$fn]:-}" ]]; then
        continue
      fi
      # Skip bash completion functions (start with _) to avoid extglob issues
      if [[ "${fn}" == _* ]]; then
        continue
      fi
      declare -f "${fn}"
      printf '\n'
    done < <(declare -F)

    printf '# Variables\n'
    local var
    while IFS= read -r var; do
      if [[ -n "${skip_lookup[$var]:-}" ]]; then
        continue
      fi
      if [[ "${var}" == BASH_FUNC_* ]]; then
        continue
      fi
      if ! declare -p "${var}" &>/dev/null; then
        continue
      fi
      declare -p "${var}"
    done < <(compgen -v)
  } >"${tmpfile}"

  chmod 600 "${tmpfile}" 2>/dev/null || true
  mv "${tmpfile}" "${outfile}"
  printf 'Environment snapshot saved to %s\n' "${outfile}"
}

cache_env
unset -f cache_env
