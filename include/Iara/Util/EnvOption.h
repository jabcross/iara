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

} // namespace iara::util

#endif
