#ifndef IARA_UTIL_ENVOPTION_H
#define IARA_UTIL_ENVOPTION_H

#include <cstdlib>
#include <string>

namespace iara::util {

// Resolve an iara-opt knob with the precedence: explicit CLI option > env var >
// fallback. MlirOptMain does not feed env vars into pass options on its own, so
// passes call this to make their options also settable from the environment
// (e.g. so the build can drive them without rewriting the pass-pipeline string).
//
//   cli_set   : whether the CLI option was given (e.g. opt.hasValue()).
//   cli_value : the CLI option value (used only when cli_set).
//   env_name  : environment variable consulted when the CLI option is absent.
//   fallback  : value when neither CLI nor env is set.
inline std::string optionOrEnv(bool cli_set, const std::string &cli_value,
                               const char *env_name,
                               const std::string &fallback = "") {
  if (cli_set)
    return cli_value;
  if (const char *e = std::getenv(env_name); e && *e)
    return std::string(e);
  return fallback;
}

// Single source of truth for the effective alloc mode. data-triggered is the
// default (priming is DEPRECATED); the compiler feature gates (zero-copy borrow,
// pingpong feedback, delay-borrow) all key on this so they agree with the build
// default when IARA_ALLOC_MODE is unset. Env-based: the experiment framework
// drives alloc mode via IARA_ALLOC_MODE; a CLI-only --alloc-mode override is not
// reflected here (unused by the framework).
inline bool dataTriggeredActive() {
  return optionOrEnv(false, "", "IARA_ALLOC_MODE", "data-triggered") ==
         "data-triggered";
}

// Single source of truth for the zero-copy read-only broadcast (RO-borrow).
// Default ON (join-owns-buffer), toggle off with IARA_BROADCAST_OWNERSHIP=
// copy-all-but-one. Requires data-triggered alloc: borrowers alias one buffer
// across per-iteration readers, which priming's reused buffers cannot provide.
// When off (priming or explicit copy-all-but-one) the broadcast falls back to
// the copy path — still correct, only more memory + slower.
inline bool borrowModeActive() {
  return dataTriggeredActive() &&
         optionOrEnv(false, "", "IARA_BROADCAST_OWNERSHIP",
                     "join-owns-buffer") == "join-owns-buffer";
}

} // namespace iara::util

#endif
