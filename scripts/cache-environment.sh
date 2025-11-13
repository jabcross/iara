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
