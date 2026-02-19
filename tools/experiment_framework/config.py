#!/usr/bin/env python3
"""
Configuration module for IaRa Unified Experiment Framework.

This module provides YAML parsing, validation, hashing, and parameter
combination generation for experiment configurations.
"""

import hashlib
import json
import logging
from pathlib import Path
from typing import Dict, List, Any, Optional
import yaml


# Configure logging
logger = logging.getLogger(__name__)


class ConfigError(Exception):
    """Exception raised for configuration-related errors."""
    pass


class ValidationError(Exception):
    """Exception raised for YAML schema validation errors."""
    pass


def load_experiments_yaml(yaml_path: Path) -> Dict[str, Any]:
    """
    Load and validate experiments.yaml file.

    Args:
        yaml_path: Path to the experiments.yaml file

    Returns:
        Parsed configuration dictionary

    Raises:
        ConfigError: If file doesn't exist or YAML syntax is invalid
        ValidationError: If schema validation fails
    """
    # Check file exists
    if not yaml_path.exists():
        raise ConfigError(f"Configuration file not found: {yaml_path}")

    # Load YAML
    try:
        with open(yaml_path, 'r') as f:
            config = yaml.safe_load(f)
    except yaml.YAMLError as e:
        raise ConfigError(f"Invalid YAML syntax in {yaml_path}: {e}")
    except Exception as e:
        raise ConfigError(f"Error reading {yaml_path}: {e}")

    # Validate schema
    _validate_schema(config, yaml_path)

    return config


def _validate_schema(config: Dict[str, Any], yaml_path: Path) -> None:
    """
    Validate that the configuration has required top-level keys and correct types.

    Args:
        config: Parsed configuration dictionary
        yaml_path: Path to configuration file (for error messages)

    Raises:
        ValidationError: If schema validation fails
    """
    if not isinstance(config, dict):
        raise ValidationError(f"{yaml_path}: Root element must be a dictionary")

    # Required top-level keys with expected types
    required_keys = {
        'application': dict,
        'parameters': list,
        'experiment_sets': list,
    }

    # Optional top-level keys with expected types
    optional_keys = {
        'measurements': list,         # Optional: not needed for generate/build
        'computed_parameters': list,
        'constraints': list,
        'execution': dict,
        'visualization': dict,
        'output': dict,
    }

    # Check for required keys
    for key, expected_type in required_keys.items():
        if key not in config:
            raise ValidationError(f"{yaml_path}: Missing required top-level key: '{key}'")

        if not isinstance(config[key], expected_type):
            actual_type = type(config[key]).__name__
            expected_type_name = expected_type.__name__
            raise ValidationError(
                f"{yaml_path}: Key '{key}' must be of type {expected_type_name}, "
                f"got {actual_type}"
            )

    # Validate optional keys if present
    for key, expected_type in optional_keys.items():
        if key in config and config[key] is not None and not isinstance(config[key], expected_type):
            actual_type = type(config[key]).__name__
            expected_type_name = expected_type.__name__
            raise ValidationError(
                f"{yaml_path}: Key '{key}' must be of type {expected_type_name}, "
                f"got {actual_type}"
            )

    logger.debug(f"Schema validation passed for {yaml_path}")


def _normalize_yaml_for_hashing(yaml_dict: Dict[str, Any]) -> str:
    """
    Normalize a parsed YAML dictionary for deterministic hashing.

    Normalization ensures that:
    - Comments are removed (automatic from parsing)
    - Whitespace is normalized (automatic from dict structure)
    - Keys are sorted consistently (via JSON sort_keys)
    - Result is independent of original YAML formatting

    Args:
        yaml_dict: Parsed YAML configuration dictionary

    Returns:
        JSON string with sorted keys and compact formatting

    Examples:
        >>> yaml1 = {'scheduler': 'vf-omp', 'matrix_size': 2048, 'num_blocks': 2}
        >>> yaml2 = {'num_blocks': 2, 'scheduler': 'vf-omp', 'matrix_size': 2048}
        >>> _normalize_yaml_for_hashing(yaml1) == _normalize_yaml_for_hashing(yaml2)
        True
    """
    return json.dumps(yaml_dict, sort_keys=True, separators=(',', ':'))


def compute_yaml_hash(yaml_path: Path) -> str:
    """
    Compute SHA256 hash of YAML content (normalized).

    Normalization:
        - Strip comments
        - Normalize whitespace
        - Sort keys for deterministic hashing

    Args:
        yaml_path: Path to the YAML file

    Returns:
        Hex-encoded SHA256 hash (64 characters)

    Raises:
        ConfigError: If file cannot be read or is invalid YAML
        ValidationError: If schema validation fails

    Examples:
        >>> from pathlib import Path
        >>> hash_value = compute_yaml_hash(Path('applications/05-cholesky/experiment/experiments.yaml'))
        >>> len(hash_value)
        64
        >>> all(c in '0123456789abcdef' for c in hash_value)
        True
    """
    try:
        config = load_experiments_yaml(yaml_path)
    except (ConfigError, ValidationError) as e:
        logger.error(f"Failed to compute hash for {yaml_path}: {e}")
        raise

    normalized = _normalize_yaml_for_hashing(config)
    hash_obj = hashlib.sha256(normalized.encode('utf-8'))
    hash_value = hash_obj.hexdigest()

    logger.debug(f"Computed hash for {yaml_path}: {hash_value}")
    return hash_value


def check_yaml_changed(yaml_path: Path, hash_file: Path) -> bool:
    """
    Check if YAML content has changed since last generation.

    Compares the SHA256 hash of the current YAML with the stored hash.
    If hash file doesn't exist, returns True (treat as changed).
    If hash file is corrupted (not 64 hex characters), returns True (treat as changed).

    Args:
        yaml_path: Path to the YAML file
        hash_file: Path to the .experiments.yaml.hash file

    Returns:
        True if:
            - Hash file doesn't exist
            - Hash file is corrupted
            - Current hash differs from stored hash
        False if hashes match

    Raises:
        ConfigError: If YAML cannot be read or is invalid
        ValidationError: If schema validation fails

    Examples:
        >>> from pathlib import Path
        >>> import tempfile
        >>> # First call - no hash file exists
        >>> with tempfile.TemporaryDirectory() as tmp:
        ...     hash_file = Path(tmp) / '.experiments.yaml.hash'
        ...     yaml_path = Path('applications/05-cholesky/experiment/experiments.yaml')
        ...     result = check_yaml_changed(yaml_path, hash_file)
        ...     # Returns True because hash file doesn't exist yet
        ...     result
        True
    """
    # If hash file doesn't exist, treat as changed
    if not hash_file.exists():
        logger.debug(f"Hash file doesn't exist: {hash_file}")
        return True

    # Try to read stored hash
    try:
        stored_hash = hash_file.read_text().strip()
    except Exception as e:
        logger.warning(f"Failed to read hash file {hash_file}: {e}")
        return True

    # Validate stored hash format (should be 64 hex characters)
    if len(stored_hash) != 64 or not all(c in '0123456789abcdef' for c in stored_hash):
        logger.warning(f"Hash file contains invalid hash format: {hash_file}")
        return True

    # Compute current hash and compare
    try:
        current_hash = compute_yaml_hash(yaml_path)
    except (ConfigError, ValidationError) as e:
        logger.error(f"Failed to compute current hash: {e}")
        raise

    changed = stored_hash != current_hash
    if changed:
        logger.debug(f"YAML content has changed (stored: {stored_hash[:8]}..., current: {current_hash[:8]}...)")
    else:
        logger.debug(f"YAML content unchanged")

    return changed


def store_yaml_hash(yaml_path: Path, hash_file: Path) -> None:
    """
    Write computed hash to .experiments.yaml.hash file.

    Computes the SHA256 hash of the YAML file and writes it to the hash file.
    Creates parent directories and overwrites existing hash files.

    Args:
        yaml_path: Path to the YAML file
        hash_file: Path to the .experiments.yaml.hash file

    Raises:
        ConfigError: If YAML cannot be read or is invalid
        ValidationError: If schema validation fails

    Examples:
        >>> from pathlib import Path
        >>> import tempfile
        >>> with tempfile.TemporaryDirectory() as tmp:
        ...     yaml_path = Path('applications/05-cholesky/experiment/experiments.yaml')
        ...     hash_file = Path(tmp) / '.experiments.yaml.hash'
        ...     store_yaml_hash(yaml_path, hash_file)
        ...     # Hash file now exists with 64-char hash
        ...     hash_file.exists()
        True
    """
    try:
        hash_value = compute_yaml_hash(yaml_path)
    except (ConfigError, ValidationError) as e:
        logger.error(f"Failed to store hash: {e}")
        raise

    # Create parent directories if needed
    hash_file.parent.mkdir(parents=True, exist_ok=True)

    # Write hash to file with newline
    try:
        hash_file.write_text(hash_value + '\n')
        logger.debug(f"Stored hash for {yaml_path} to {hash_file}")
    except Exception as e:
        logger.error(f"Failed to write hash file {hash_file}: {e}")
        raise ConfigError(f"Failed to write hash file {hash_file}: {e}")


def get_parameter_combinations(
    config: Dict[str, Any],
    experiment_set: str
) -> List[Dict[str, Any]]:
    """
    Generate all valid parameter combinations for an experiment set.

    Process:
        1. Extract parameter definitions from config
        2. Get parameter values from specified experiment set
        3. Compute Cartesian product of all parameter values
        4. Apply constraints to filter invalid combinations
        5. Compute derived parameters for each combination

    Args:
        config: Parsed configuration dictionary
        experiment_set: Name of the experiment set to generate combinations for

    Returns:
        List of parameter dictionaries, each representing one instance.
        Each dict contains all original parameters plus computed parameters.

    Raises:
        ConfigError: If experiment set not found, constraint evaluation fails,
                    or computed parameter evaluation fails

    Examples:
        >>> config = {
        ...     'parameters': [
        ...         {'name': 'scheduler', 'type': 'str'},
        ...         {'name': 'size', 'type': 'int'},
        ...     ],
        ...     'experiment_sets': [{
        ...         'name': 'test',
        ...         'parameters': {
        ...             'scheduler': ['a', 'b'],
        ...             'size': [1, 2],
        ...         }
        ...     }],
        ...     'constraints': [],
        ...     'computed_parameters': [],
        ... }
        >>> combos = get_parameter_combinations(config, 'test')
        >>> len(combos)
        4
        >>> combos[0]
        {'scheduler': 'a', 'size': 1}
    """
    from itertools import product

    # Step 1: Extract parameter definitions
    param_defs = {}
    for param in config.get('parameters', []):
        param_defs[param['name']] = param.get('type', 'str')

    # Step 2: Find the experiment set
    exp_set = None
    for es in config.get('experiment_sets', []):
        if es['name'] == experiment_set:
            exp_set = es
            break

    if exp_set is None:
        raise ConfigError(f"Experiment set '{experiment_set}' not found in configuration")

    # Step 3: Get parameter values for this experiment set
    param_values = exp_set.get('parameters', {})

    if not param_values:
        logger.warning(f"Experiment set '{experiment_set}' has no parameters")
        return []

    # Step 4: Compute Cartesian product
    param_names = list(param_values.keys())
    param_lists = [param_values[name] for name in param_names]

    combinations = []
    for values in product(*param_lists):
        combination = dict(zip(param_names, values))
        combinations.append(combination)

    logger.debug(f"Generated {len(combinations)} raw combinations from Cartesian product")

    # Step 5: Compute derived parameters FIRST (before constraints)
    # This allows constraints to reference computed parameters
    computed_parameters = config.get('computed_parameters', [])
    combinations_with_computed = []

    for combo in combinations:
        extended = combo.copy()

        for computed in computed_parameters:
            name = computed['name']
            expr = computed.get('expression', '')
            comp_type = computed.get('type', 'str')

            try:
                # Evaluate computed parameter with restricted builtins
                value = eval(expr, {"__builtins__": {}}, extended)

                # Type conversion/validation
                if comp_type == 'int':
                    value = int(value)
                elif comp_type == 'float':
                    value = float(value)
                elif comp_type == 'bool':
                    if isinstance(value, bool):
                        pass
                    elif isinstance(value, str):
                        value = value.lower() in ('true', '1', 'yes')
                    else:
                        value = bool(value)
                elif comp_type == 'str':
                    value = str(value)

                extended[name] = value
                logger.debug(f"Computed parameter '{name}' = {value}")

            except NameError as e:
                # Parameter referenced in computed expression not found
                param_name = str(e).split("'")[1] if "'" in str(e) else "unknown"
                raise ConfigError(
                    f"Computed parameter '{name}' references unknown parameter '{param_name}': {expr}"
                )
            except (ValueError, TypeError) as e:
                raise ConfigError(
                    f"Failed to compute parameter '{name}' or convert to type '{comp_type}': {e}"
                )
            except Exception as e:
                raise ConfigError(
                    f"Failed to evaluate computed parameter expression '{expr}': {e}"
                )

        combinations_with_computed.append(extended)

    # Step 6: Apply constraints (now can reference computed parameters)
    constraints = config.get('constraints', [])
    filtered_combinations = []

    for combo in combinations_with_computed:
        valid = True
        for constraint in constraints:
            expr = constraint.get('expression', '')
            try:
                # Evaluate constraint with restricted builtins
                result = eval(expr, {"__builtins__": {}}, combo)
                if not result:
                    valid = False
                    desc = constraint.get('description', expr)
                    logger.debug(f"Constraint failed: {desc}")
                    break
            except NameError as e:
                # Parameter referenced in constraint not found
                param_name = str(e).split("'")[1] if "'" in str(e) else "unknown"
                raise ConfigError(
                    f"Constraint references unknown parameter '{param_name}': {expr}"
                )
            except Exception as e:
                raise ConfigError(
                    f"Failed to evaluate constraint expression '{expr}': {e}"
                )

        if valid:
            filtered_combinations.append(combo)

    logger.debug(f"After applying {len(constraints)} constraints: "
                f"{len(filtered_combinations)} valid combinations")

    result = filtered_combinations

    logger.info(f"Generated {len(result)} parameter combinations for experiment set '{experiment_set}'")
    return result
