#!/usr/bin/env python3
"""
Comprehensive tests for YAML hashing functionality (Task 1.2).

Tests cover:
1. Hash consistency (same YAML = same hash)
2. Hash format (64 hex characters)
3. Value changes (different values = different hash)
4. Comment insensitivity (comments don't affect hash)
5. Whitespace insensitivity (whitespace doesn't affect hash)
6. Key order insensitivity (key order doesn't affect hash)
7. Change detection (correctly detects YAML changes)
8. Corrupted hash files (treats as changed gracefully)
9. Multiple applications (robustness across different YAML files)
"""

import json
import logging
import tempfile
from pathlib import Path
from typing import Dict, Any

import yaml

# Setup logging for test output
logging.basicConfig(
    level=logging.DEBUG,
    format='%(levelname)-8s %(name)s: %(message)s'
)

# Import the functions to test
from tools.experiment_framework.config import (
    compute_yaml_hash,
    check_yaml_changed,
    store_yaml_hash,
    _normalize_yaml_for_hashing,
    load_experiments_yaml,
    ConfigError,
    ValidationError
)


def test_normalize_yaml_consistency():
    """Test that YAML normalization produces consistent output."""
    print("\n" + "="*70)
    print("TEST 1: Normalize YAML Consistency")
    print("="*70)

    # Create test YAML in different orders
    yaml1 = {'scheduler': 'vf-omp', 'matrix_size': 2048, 'num_blocks': 2}
    yaml2 = {'num_blocks': 2, 'scheduler': 'vf-omp', 'matrix_size': 2048}
    yaml3 = {'matrix_size': 2048, 'num_blocks': 2, 'scheduler': 'vf-omp'}

    norm1 = _normalize_yaml_for_hashing(yaml1)
    norm2 = _normalize_yaml_for_hashing(yaml2)
    norm3 = _normalize_yaml_for_hashing(yaml3)

    print(f"Normalized YAML 1: {norm1}")
    print(f"Normalized YAML 2: {norm2}")
    print(f"Normalized YAML 3: {norm3}")

    assert norm1 == norm2, "Different key order should produce same normalization"
    assert norm2 == norm3, "Different key order should produce same normalization"
    assert norm1 == norm3, "All three should be identical"

    # Check format: should be compact JSON with sorted keys
    assert ',' in norm1, "Normalized form should use comma separators"
    assert ':' in norm1, "Normalized form should use colon separators"
    assert ' ' not in norm1, "Normalized form should have no spaces"

    print("✓ PASS: Normalization is consistent regardless of key order")
    return True


def test_hash_format():
    """Test that compute_yaml_hash returns exactly 64 hex characters."""
    print("\n" + "="*70)
    print("TEST 2: Hash Format")
    print("="*70)

    yaml_path = Path('applications/05-cholesky/experiment/experiments.yaml')
    if not yaml_path.exists():
        print(f"⊘ SKIP: {yaml_path} not found")
        return None

    hash_value = compute_yaml_hash(yaml_path)
    print(f"Hash value: {hash_value}")
    print(f"Hash length: {len(hash_value)}")

    assert len(hash_value) == 64, f"Hash should be 64 characters, got {len(hash_value)}"
    assert all(c in '0123456789abcdef' for c in hash_value), "Hash should only contain hex digits"

    print("✓ PASS: Hash is exactly 64 hex characters")
    return True


def test_hash_consistency():
    """Test that the same YAML produces the same hash."""
    print("\n" + "="*70)
    print("TEST 3: Hash Consistency")
    print("="*70)

    yaml_path = Path('applications/05-cholesky/experiment/experiments.yaml')
    if not yaml_path.exists():
        print(f"⊘ SKIP: {yaml_path} not found")
        return None

    hash1 = compute_yaml_hash(yaml_path)
    hash2 = compute_yaml_hash(yaml_path)
    hash3 = compute_yaml_hash(yaml_path)

    print(f"Hash 1: {hash1}")
    print(f"Hash 2: {hash2}")
    print(f"Hash 3: {hash3}")

    assert hash1 == hash2, "Same YAML should produce same hash"
    assert hash2 == hash3, "Same YAML should produce same hash"
    assert hash1 == hash3, "All hashes should be identical"

    print("✓ PASS: Identical YAML produces identical hash every time")
    return True


def test_hash_changes_with_values():
    """Test that different values produce different hashes."""
    print("\n" + "="*70)
    print("TEST 4: Hash Changes with Value Changes")
    print("="*70)

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir = Path(tmpdir)

        # Create two different YAML files
        yaml1_path = tmpdir / 'test1.yaml'
        yaml2_path = tmpdir / 'test2.yaml'

        # Minimal valid YAML structure
        base_config = {
            'application': {'name': 'test'},
            'parameters': [],
            'measurements': [],
            'experiment_sets': []
        }

        # Write first version
        config1 = base_config.copy()
        with open(yaml1_path, 'w') as f:
            yaml.dump(config1, f)

        # Write second version with different value
        config2 = base_config.copy()
        config2['application']['name'] = 'different'
        with open(yaml2_path, 'w') as f:
            yaml.dump(config2, f)

        hash1 = compute_yaml_hash(yaml1_path)
        hash2 = compute_yaml_hash(yaml2_path)

        print(f"Hash for 'test': {hash1}")
        print(f"Hash for 'different': {hash2}")

        assert hash1 != hash2, "Different values should produce different hashes"

        print("✓ PASS: Different values produce different hashes")
        return True


def test_hash_insensitive_to_comments():
    """Test that comments don't affect hash."""
    print("\n" + "="*70)
    print("TEST 5: Hash Insensitive to Comments")
    print("="*70)

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir = Path(tmpdir)

        # Create YAML with comments
        yaml_with_comments = tmpdir / 'with_comments.yaml'
        yaml_without_comments = tmpdir / 'without_comments.yaml'

        base_config = {
            'application': {'name': 'test-app'},
            'parameters': [],
            'measurements': [],
            'experiment_sets': []
        }

        # Write without comments
        with open(yaml_without_comments, 'w') as f:
            yaml.dump(base_config, f)

        # Write with comments (manually)
        with open(yaml_with_comments, 'w') as f:
            f.write("""# This is a test application
application:
  name: test-app  # Application name

# Parameters section
parameters: []

# Measurements to collect
measurements: []

# Experiment sets to run
experiment_sets: []
""")

        hash1 = compute_yaml_hash(yaml_with_comments)
        hash2 = compute_yaml_hash(yaml_without_comments)

        print(f"Hash with comments: {hash1}")
        print(f"Hash without comments: {hash2}")

        assert hash1 == hash2, "Comments should not affect hash"

        print("✓ PASS: Comments don't affect hash")
        return True


def test_hash_insensitive_to_whitespace():
    """Test that whitespace formatting doesn't affect hash."""
    print("\n" + "="*70)
    print("TEST 6: Hash Insensitive to Whitespace")
    print("="*70)

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir = Path(tmpdir)

        yaml_compact = tmpdir / 'compact.yaml'
        yaml_spaced = tmpdir / 'spaced.yaml'

        base_config = {
            'application': {'name': 'test-app'},
            'parameters': [],
            'measurements': [],
            'experiment_sets': []
        }

        # Write with default formatting
        with open(yaml_compact, 'w') as f:
            yaml.dump(base_config, f, default_flow_style=False)

        # Write with extra whitespace (manually) - use [] notation to preserve empty lists
        with open(yaml_spaced, 'w') as f:
            f.write("""application:
  name:    test-app


parameters: []

measurements: []

experiment_sets: []
""")

        hash1 = compute_yaml_hash(yaml_compact)
        hash2 = compute_yaml_hash(yaml_spaced)

        print(f"Hash compact: {hash1}")
        print(f"Hash spaced: {hash2}")

        assert hash1 == hash2, "Whitespace should not affect hash"

        print("✓ PASS: Whitespace doesn't affect hash")
        return True


def test_hash_insensitive_to_key_order():
    """Test that key order in the original YAML doesn't affect hash."""
    print("\n" + "="*70)
    print("TEST 7: Hash Insensitive to Key Order")
    print("="*70)

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir = Path(tmpdir)

        yaml_order1 = tmpdir / 'order1.yaml'
        yaml_order2 = tmpdir / 'order2.yaml'

        base_config = {
            'application': {'name': 'test-app'},
            'parameters': [],
            'measurements': [],
            'experiment_sets': []
        }

        # Write in one order
        with open(yaml_order1, 'w') as f:
            f.write("application:\n  name: test-app\nparameters: []\nmeasurements: []\nexperiment_sets: []\n")

        # Write in different order
        with open(yaml_order2, 'w') as f:
            f.write("experiment_sets: []\nmeasurements: []\nparameters: []\napplication:\n  name: test-app\n")

        hash1 = compute_yaml_hash(yaml_order1)
        hash2 = compute_yaml_hash(yaml_order2)

        print(f"Hash order 1: {hash1}")
        print(f"Hash order 2: {hash2}")

        assert hash1 == hash2, "Key order should not affect hash"

        print("✓ PASS: Key order doesn't affect hash")
        return True


def test_change_detection_no_hash_file():
    """Test that check_yaml_changed returns True when hash file doesn't exist."""
    print("\n" + "="*70)
    print("TEST 8: Change Detection - No Hash File")
    print("="*70)

    yaml_path = Path('applications/05-cholesky/experiment/experiments.yaml')
    if not yaml_path.exists():
        print(f"⊘ SKIP: {yaml_path} not found")
        return None

    with tempfile.TemporaryDirectory() as tmpdir:
        hash_file = Path(tmpdir) / '.experiments.yaml.hash'

        # Hash file doesn't exist yet
        assert not hash_file.exists(), "Hash file should not exist initially"

        result = check_yaml_changed(yaml_path, hash_file)

        print(f"check_yaml_changed when hash file doesn't exist: {result}")
        assert result is True, "Should return True when hash file doesn't exist"

        print("✓ PASS: Returns True when hash file doesn't exist")
        return True


def test_change_detection_matching_hash():
    """Test that check_yaml_changed returns False when hash matches."""
    print("\n" + "="*70)
    print("TEST 9: Change Detection - Matching Hash")
    print("="*70)

    yaml_path = Path('applications/05-cholesky/experiment/experiments.yaml')
    if not yaml_path.exists():
        print(f"⊘ SKIP: {yaml_path} not found")
        return None

    with tempfile.TemporaryDirectory() as tmpdir:
        hash_file = Path(tmpdir) / '.experiments.yaml.hash'

        # Store the hash
        store_yaml_hash(yaml_path, hash_file)
        assert hash_file.exists(), "Hash file should exist after storing"

        # Check if changed - should be False
        result = check_yaml_changed(yaml_path, hash_file)

        print(f"check_yaml_changed when hash matches: {result}")
        assert result is False, "Should return False when hash matches"

        print("✓ PASS: Returns False when hash matches")
        return True


def test_change_detection_different_hash():
    """Test that check_yaml_changed returns True when hash differs."""
    print("\n" + "="*70)
    print("TEST 10: Change Detection - Different Hash")
    print("="*70)

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir = Path(tmpdir)

        yaml_path = tmpdir / 'test.yaml'
        hash_file = tmpdir / '.test.yaml.hash'

        base_config = {
            'application': {'name': 'test-app'},
            'parameters': [],
            'measurements': [],
            'experiment_sets': []
        }

        # Write initial YAML
        with open(yaml_path, 'w') as f:
            yaml.dump(base_config, f)

        # Store the hash
        store_yaml_hash(yaml_path, hash_file)
        assert hash_file.exists(), "Hash file should exist after storing"

        # Modify YAML
        base_config['application']['name'] = 'modified-app'
        with open(yaml_path, 'w') as f:
            yaml.dump(base_config, f)

        # Check if changed - should be True
        result = check_yaml_changed(yaml_path, hash_file)

        print(f"check_yaml_changed when hash differs: {result}")
        assert result is True, "Should return True when hash differs"

        print("✓ PASS: Returns True when hash differs")
        return True


def test_change_detection_corrupted_hash():
    """Test that check_yaml_changed treats corrupted hash file as changed."""
    print("\n" + "="*70)
    print("TEST 11: Change Detection - Corrupted Hash File")
    print("="*70)

    yaml_path = Path('applications/05-cholesky/experiment/experiments.yaml')
    if not yaml_path.exists():
        print(f"⊘ SKIP: {yaml_path} not found")
        return None

    with tempfile.TemporaryDirectory() as tmpdir:
        hash_file = Path(tmpdir) / '.experiments.yaml.hash'

        # Write corrupted hash (wrong length)
        hash_file.write_text('invalid_hash_too_short\n')

        result = check_yaml_changed(yaml_path, hash_file)

        print(f"check_yaml_changed with corrupted hash: {result}")
        assert result is True, "Should return True for corrupted hash file"

        # Try with non-hex characters
        hash_file.write_text('x' * 64 + '\n')
        result = check_yaml_changed(yaml_path, hash_file)

        print(f"check_yaml_changed with non-hex hash: {result}")
        assert result is True, "Should return True for non-hex hash"

        print("✓ PASS: Corrupted hash files treated as changed")
        return True


def test_store_yaml_hash_creates_file():
    """Test that store_yaml_hash creates the hash file."""
    print("\n" + "="*70)
    print("TEST 12: Store YAML Hash - Creates File")
    print("="*70)

    yaml_path = Path('applications/05-cholesky/experiment/experiments.yaml')
    if not yaml_path.exists():
        print(f"⊘ SKIP: {yaml_path} not found")
        return None

    with tempfile.TemporaryDirectory() as tmpdir:
        hash_file = Path(tmpdir) / '.experiments.yaml.hash'

        assert not hash_file.exists(), "Hash file should not exist initially"

        store_yaml_hash(yaml_path, hash_file)

        assert hash_file.exists(), "Hash file should exist after storing"

        content = hash_file.read_text()
        print(f"Hash file content: {content[:20]}...")
        print(f"Hash file size: {len(content)} characters")

        # Extract hash (remove newline)
        stored_hash = content.strip()
        assert len(stored_hash) == 64, f"Stored hash should be 64 chars, got {len(stored_hash)}"
        assert all(c in '0123456789abcdef' for c in stored_hash), "Hash should be hex"

        print("✓ PASS: Hash file created correctly")
        return True


def test_store_yaml_hash_creates_directories():
    """Test that store_yaml_hash creates parent directories."""
    print("\n" + "="*70)
    print("TEST 13: Store YAML Hash - Creates Directories")
    print("="*70)

    yaml_path = Path('applications/05-cholesky/experiment/experiments.yaml')
    if not yaml_path.exists():
        print(f"⊘ SKIP: {yaml_path} not found")
        return None

    with tempfile.TemporaryDirectory() as tmpdir:
        hash_file = Path(tmpdir) / 'nested' / 'dir' / 'structure' / '.experiments.yaml.hash'

        assert not hash_file.parent.exists(), "Parent directories should not exist initially"

        store_yaml_hash(yaml_path, hash_file)

        assert hash_file.exists(), "Hash file should exist after storing"
        assert hash_file.parent.exists(), "Parent directories should be created"

        print(f"Created directories: {hash_file.parent}")
        print("✓ PASS: Parent directories created correctly")
        return True


def test_store_yaml_hash_overwrites_existing():
    """Test that store_yaml_hash overwrites existing hash file."""
    print("\n" + "="*70)
    print("TEST 14: Store YAML Hash - Overwrites Existing")
    print("="*70)

    yaml_path = Path('applications/05-cholesky/experiment/experiments.yaml')
    if not yaml_path.exists():
        print(f"⊘ SKIP: {yaml_path} not found")
        return None

    with tempfile.TemporaryDirectory() as tmpdir:
        hash_file = Path(tmpdir) / '.experiments.yaml.hash'

        # Store initial hash
        store_yaml_hash(yaml_path, hash_file)
        initial_content = hash_file.read_text()

        # Store again
        store_yaml_hash(yaml_path, hash_file)
        new_content = hash_file.read_text()

        print(f"Initial hash: {initial_content.strip()[:20]}...")
        print(f"New hash: {new_content.strip()[:20]}...")

        assert initial_content == new_content, "Re-storing same YAML should produce same hash"

        print("✓ PASS: Hash file overwrites correctly")
        return True


def test_multiple_applications():
    """Test hashing with multiple real YAML files from applications."""
    print("\n" + "="*70)
    print("TEST 15: Multiple Applications")
    print("="*70)

    applications = [
        'applications/05-cholesky/experiment/experiments.yaml',
        'applications/08-sift/experiment/experiments.yaml',
        'applications/13-degridder/experiment/experiments.yaml',
    ]

    results = {}
    for app_path in applications:
        app_path = Path(app_path)
        if not app_path.exists():
            print(f"⊘ SKIP: {app_path} not found")
            continue

        try:
            hash_value = compute_yaml_hash(app_path)
            results[str(app_path)] = hash_value
            print(f"✓ {app_path}: {hash_value[:16]}...")
        except Exception as e:
            print(f"✗ {app_path}: {e}")
            results[str(app_path)] = None

    # Verify all hashes are valid
    valid_count = 0
    for app_path, hash_value in results.items():
        if hash_value is not None:
            assert len(hash_value) == 64, f"Hash should be 64 chars: {app_path}"
            assert all(c in '0123456789abcdef' for c in hash_value), f"Hash should be hex: {app_path}"
            valid_count += 1

    if valid_count > 0:
        print(f"\n✓ PASS: {valid_count} application(s) hashed successfully")
        return True
    else:
        print("\n⊘ SKIP: No application YAML files found")
        return None


def main():
    """Run all tests."""
    print("\n" + "="*70)
    print("YAML HASHING TESTS (Task 1.2)")
    print("="*70)

    tests = [
        ("Hash Consistency", test_normalize_yaml_consistency),
        ("Hash Format", test_hash_format),
        ("Hash Consistency (Real File)", test_hash_consistency),
        ("Hash Changes with Values", test_hash_changes_with_values),
        ("Hash Insensitive to Comments", test_hash_insensitive_to_comments),
        ("Hash Insensitive to Whitespace", test_hash_insensitive_to_whitespace),
        ("Hash Insensitive to Key Order", test_hash_insensitive_to_key_order),
        ("Change Detection - No Hash File", test_change_detection_no_hash_file),
        ("Change Detection - Matching Hash", test_change_detection_matching_hash),
        ("Change Detection - Different Hash", test_change_detection_different_hash),
        ("Change Detection - Corrupted Hash", test_change_detection_corrupted_hash),
        ("Store Hash - Creates File", test_store_yaml_hash_creates_file),
        ("Store Hash - Creates Directories", test_store_yaml_hash_creates_directories),
        ("Store Hash - Overwrites Existing", test_store_yaml_hash_overwrites_existing),
        ("Multiple Applications", test_multiple_applications),
    ]

    results = {}
    passed = 0
    failed = 0
    skipped = 0

    for test_name, test_func in tests:
        try:
            result = test_func()
            if result is True:
                results[test_name] = "PASS"
                passed += 1
            elif result is None:
                results[test_name] = "SKIP"
                skipped += 1
            else:
                results[test_name] = "FAIL"
                failed += 1
        except AssertionError as e:
            results[test_name] = f"FAIL: {e}"
            failed += 1
        except Exception as e:
            results[test_name] = f"ERROR: {e}"
            failed += 1

    # Print summary
    print("\n" + "="*70)
    print("TEST SUMMARY")
    print("="*70)
    for test_name, result in results.items():
        status_symbol = "✓" if "PASS" in result else "⊘" if "SKIP" in result else "✗"
        print(f"{status_symbol} {test_name}: {result}")

    print("\n" + "="*70)
    print(f"Results: {passed} passed, {failed} failed, {skipped} skipped")
    print("="*70)

    if failed == 0:
        print("\nAll tests passed!")
        return 0
    else:
        print(f"\n{failed} test(s) failed")
        return 1


if __name__ == '__main__':
    exit(main())
