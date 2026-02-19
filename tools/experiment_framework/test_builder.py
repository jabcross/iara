"""
Unit tests for builder.py - Testing retry logic, error extraction, and failure tracking.
"""

import pytest
import tempfile
import time
from pathlib import Path
from unittest.mock import Mock, patch, MagicMock
from datetime import datetime, timezone

from builder import (
    extract_build_errors,
    FailedBuildResult,
    BuildResults,
    BuildResult,
    build_instance,
    build_all_instances,
)


class TestExtractBuildErrors:
    """Tests for extract_build_errors function."""

    def test_extract_compiler_error(self):
        """Test extraction of compiler error messages."""
        log = """
        Compiling main.c...
        main.c:10: error: undefined reference to 'function_name'
        /usr/bin/ld: failed
        """
        result = extract_build_errors(log)
        assert result['phase'] == 'build'
        assert result['step'] == 'clang'
        assert 'undefined reference' in result['message']
        assert isinstance(result['context'], list)
        assert len(result['context']) > 0

    def test_extract_cmake_error(self):
        """Test extraction of CMake configuration errors."""
        log = """
        CMake Error at CMakeLists.txt:42 (find_package):
          Could not find the package XYZ
        """
        result = extract_build_errors(log)
        assert result['phase'] == 'configuration'
        assert result['step'] == 'cmake'
        assert 'CMake Error' in result['message']

    def test_extract_iara_opt_error(self):
        """Test extraction of IARA optimization errors."""
        log = """
        Running iara-opt...
        error: Invalid optimization flag
        IARA optimization failed
        """
        result = extract_build_errors(log)
        assert result['phase'] == 'build'
        assert result['step'] == 'iara-opt'

    def test_extract_mlir_error(self):
        """Test extraction of MLIR conversion errors."""
        log = """
        Converting MLIR to LLVM...
        error: mlir conversion failed
        Invalid dialect in input
        """
        result = extract_build_errors(log)
        assert result['phase'] == 'build'
        assert 'error' in result['message'].lower() or 'conversion' in result['message'].lower()

    def test_extract_fatal_error(self):
        """Test extraction of fatal errors."""
        log = """
        fatal: segmentation fault during compilation
        """
        result = extract_build_errors(log)
        assert result['phase'] == 'build'
        assert 'fatal' in result['message'].lower()

    def test_extract_test_failure(self):
        """Test extraction of test failures."""
        log = """
        Running test suite...
        Test case 1: PASSED
        Test case 2: FAILED
        """
        result = extract_build_errors(log)
        assert result['phase'] == 'build'
        assert 'FAILED' in result['message']

    def test_no_errors_in_log(self):
        """Test handling of logs with no errors."""
        log = """
        Compiling main.c...
        Building target...
        Build complete!
        """
        result = extract_build_errors(log)
        assert result == {}

    def test_empty_log(self):
        """Test handling of empty logs."""
        assert extract_build_errors("") == {}
        assert extract_build_errors(None) == {}

    def test_context_extraction(self):
        """Test that context lines are properly extracted."""
        log = """Line 1
Line 2
Line 3
error: This is the error
Line 5
Line 6"""
        result = extract_build_errors(log)
        assert result['message'] == "error: This is the error"
        # Context should include surrounding lines
        assert len(result['context']) > 1


class TestFailedBuildResult:
    """Tests for FailedBuildResult dataclass."""

    def test_creation_with_all_fields(self):
        """Test creating FailedBuildResult with all required fields."""
        errors = [
            {
                'attempt': 1,
                'phase': 'build',
                'step': 'cmake',
                'message': 'CMake configuration failed',
                'context': ['line 1', 'line 2'],
                'timestamp': '2025-01-14T15:00:00Z'
            }
        ]
        failed = FailedBuildResult(
            instance_name='test_instance',
            attempts=2,
            errors=errors,
            timestamp='2025-01-14T15:01:00Z',
            last_error='Build failed after 2 attempts'
        )
        assert failed.instance_name == 'test_instance'
        assert failed.attempts == 2
        assert len(failed.errors) == 1
        assert failed.errors[0]['phase'] == 'build'
        assert failed.last_error == 'Build failed after 2 attempts'

    def test_multiple_attempts_in_errors(self):
        """Test tracking errors from multiple retry attempts."""
        errors = [
            {
                'attempt': 1,
                'phase': 'build',
                'step': 'cmake',
                'message': 'CMake error on attempt 1',
                'context': [],
                'timestamp': '2025-01-14T15:00:00Z'
            },
            {
                'attempt': 2,
                'phase': 'build',
                'step': 'clang',
                'message': 'Linker error on attempt 2',
                'context': [],
                'timestamp': '2025-01-14T15:00:30Z'
            }
        ]
        failed = FailedBuildResult(
            instance_name='instance_x',
            attempts=2,
            errors=errors,
            timestamp='2025-01-14T15:01:00Z',
            last_error='Linker error on attempt 2'
        )
        assert len(failed.errors) == 2
        assert failed.errors[0]['attempt'] == 1
        assert failed.errors[1]['attempt'] == 2


class TestBuildResults:
    """Tests for BuildResults aggregation."""

    def test_build_results_initialization(self):
        """Test BuildResults post_init calculations."""
        successful = [
            BuildResult(
                success=True,
                instance_name='inst1',
                attempt=1,
                timestamp='2025-01-14T15:00:00Z'
            ),
            BuildResult(
                success=True,
                instance_name='inst2',
                attempt=1,
                timestamp='2025-01-14T15:00:01Z'
            )
        ]
        failed = [
            FailedBuildResult(
                instance_name='inst3',
                attempts=2,
                errors=[],
                timestamp='2025-01-14T15:00:02Z',
                last_error='Failed'
            )
        ]
        results = BuildResults(
            successful_instances=successful,
            failed_instances=failed,
            timestamp='2025-01-14T15:00:00Z'
        )
        assert results.successful_count == 2
        assert results.failed_count == 1
        assert results.total_instances == 3

    def test_all_successful(self):
        """Test BuildResults when all instances succeed."""
        successful = [
            BuildResult(
                success=True,
                instance_name=f'inst{i}',
                attempt=1,
                timestamp='2025-01-14T15:00:00Z'
            )
            for i in range(5)
        ]
        results = BuildResults(
            successful_instances=successful,
            failed_instances=[],
            timestamp='2025-01-14T15:00:00Z'
        )
        assert results.successful_count == 5
        assert results.failed_count == 0
        assert results.total_instances == 5

    def test_all_failed(self):
        """Test BuildResults when all instances fail."""
        failed = [
            FailedBuildResult(
                instance_name=f'inst{i}',
                attempts=2,
                errors=[],
                timestamp='2025-01-14T15:00:00Z',
                last_error='Failed'
            )
            for i in range(3)
        ]
        results = BuildResults(
            successful_instances=[],
            failed_instances=failed,
            timestamp='2025-01-14T15:00:00Z'
        )
        assert results.successful_count == 0
        assert results.failed_count == 3
        assert results.total_instances == 3


class TestBuildAllInstancesRetryLogic:
    """Tests for build_all_instances retry logic and error tracking."""

    @patch('builder.build_instance')
    def test_retry_on_failure_then_success(self, mock_build):
        """Test that instance retries on first failure and succeeds on second attempt."""
        # First call fails, second succeeds
        mock_build.side_effect = [
            BuildResult(
                success=False,
                instance_name='test_inst',
                attempt=1,
                timestamp='2025-01-14T15:00:00Z',
                errors=['Build error']
            ),
            BuildResult(
                success=True,
                instance_name='test_inst',
                attempt=2,
                timestamp='2025-01-14T15:00:05Z',
                compilation={'total_time_s': 10.5}
            )
        ]

        with tempfile.TemporaryDirectory() as tmpdir:
            base_dir = Path(tmpdir)
            cmake_source = base_dir / 'cmake'
            cmake_source.mkdir()

            results = build_all_instances(
                instances=['test_inst'],
                base_build_dir=base_dir,
                cmake_source=cmake_source,
                max_retries=2
            )

            # Should succeed after retry
            assert results.successful_count == 1
            assert results.failed_count == 0
            assert results.successful_instances[0].attempt == 2
            assert mock_build.call_count == 2

    @patch('builder.build_instance')
    def test_continue_on_error(self, mock_build):
        """Test that build continues even when one instance fails."""
        # Instance 1 fails, Instance 2 succeeds, Instance 3 fails
        mock_build.side_effect = [
            # Instance 1 - both attempts fail
            BuildResult(success=False, instance_name='inst1', attempt=1,
                       timestamp='2025-01-14T15:00:00Z', errors=['Error 1']),
            BuildResult(success=False, instance_name='inst1', attempt=2,
                       timestamp='2025-01-14T15:00:05Z', errors=['Error 1']),
            # Instance 2 - succeeds on first try
            BuildResult(success=True, instance_name='inst2', attempt=1,
                       timestamp='2025-01-14T15:00:10Z'),
            # Instance 3 - both attempts fail
            BuildResult(success=False, instance_name='inst3', attempt=1,
                       timestamp='2025-01-14T15:00:15Z', errors=['Error 3']),
            BuildResult(success=False, instance_name='inst3', attempt=2,
                       timestamp='2025-01-14T15:00:20Z', errors=['Error 3']),
        ]

        with tempfile.TemporaryDirectory() as tmpdir:
            base_dir = Path(tmpdir)
            cmake_source = base_dir / 'cmake'
            cmake_source.mkdir()

            results = build_all_instances(
                instances=['inst1', 'inst2', 'inst3'],
                base_build_dir=base_dir,
                cmake_source=cmake_source,
                max_retries=2
            )

            # Should have 1 successful and 2 failed
            assert results.successful_count == 1
            assert results.failed_count == 2
            assert results.total_instances == 3
            assert results.successful_instances[0].instance_name == 'inst2'

    @patch('builder.time.sleep')
    @patch('builder.build_instance')
    def test_exponential_backoff(self, mock_build, mock_sleep):
        """Test that exponential backoff delays are applied between retries."""
        # Always fail to trigger retries
        mock_build.return_value = BuildResult(
            success=False,
            instance_name='test',
            attempt=1,
            timestamp='2025-01-14T15:00:00Z',
            errors=['Error']
        )

        with tempfile.TemporaryDirectory() as tmpdir:
            base_dir = Path(tmpdir)
            cmake_source = base_dir / 'cmake'
            cmake_source.mkdir()

            results = build_all_instances(
                instances=['test'],
                base_build_dir=base_dir,
                cmake_source=cmake_source,
                max_retries=3
            )

            # Check sleep was called with exponential backoff: 1s, 2s
            # (max_retries=3 means attempt 1 fails, sleep 1s, attempt 2 fails, sleep 2s, attempt 3 fails)
            assert mock_sleep.call_count == 2
            # First sleep: 2^(1-1) = 1s
            # Second sleep: 2^(2-1) = 2s
            mock_sleep.assert_any_call(1)
            mock_sleep.assert_any_call(2)

    @patch('builder.build_instance')
    def test_error_tracking_across_attempts(self, mock_build):
        """Test that errors are properly tracked for each attempt."""
        mock_build.side_effect = [
            BuildResult(
                success=False,
                instance_name='test',
                attempt=1,
                timestamp='2025-01-14T15:00:00Z',
                errors=['CMake error on attempt 1']
            ),
            BuildResult(
                success=False,
                instance_name='test',
                attempt=2,
                timestamp='2025-01-14T15:00:05Z',
                errors=['Linker error on attempt 2']
            ),
        ]

        with tempfile.TemporaryDirectory() as tmpdir:
            base_dir = Path(tmpdir)
            cmake_source = base_dir / 'cmake'
            cmake_source.mkdir()

            results = build_all_instances(
                instances=['test'],
                base_build_dir=base_dir,
                cmake_source=cmake_source,
                max_retries=2
            )

            assert results.failed_count == 1
            failed = results.failed_instances[0]
            assert failed.instance_name == 'test'
            assert failed.attempts == 2
            # Should track both attempt errors
            assert len(failed.errors) == 2
            assert failed.errors[0]['message'] == 'CMake error on attempt 1'
            assert failed.errors[1]['message'] == 'Linker error on attempt 2'
            assert failed.last_error == 'Linker error on attempt 2'

    @patch('builder.build_instance')
    def test_max_retries_respected(self, mock_build):
        """Test that max_retries parameter is respected."""
        # Always fail
        mock_build.return_value = BuildResult(
            success=False,
            instance_name='test',
            attempt=1,
            timestamp='2025-01-14T15:00:00Z',
            errors=['Error']
        )

        with tempfile.TemporaryDirectory() as tmpdir:
            base_dir = Path(tmpdir)
            cmake_source = base_dir / 'cmake'
            cmake_source.mkdir()

            with patch('builder.time.sleep'):
                results = build_all_instances(
                    instances=['test'],
                    base_build_dir=base_dir,
                    cmake_source=cmake_source,
                    max_retries=3
                )

            # Should try exactly 3 times
            assert mock_build.call_count == 3
            failed = results.failed_instances[0]
            assert failed.attempts == 3

    @patch('builder.build_instance')
    def test_single_instance_success(self, mock_build):
        """Test successful build of single instance."""
        mock_build.return_value = BuildResult(
            success=True,
            instance_name='test',
            attempt=1,
            timestamp='2025-01-14T15:00:00Z',
            compilation={'total_time_s': 5.0},
            binary_size_bytes=1024
        )

        with tempfile.TemporaryDirectory() as tmpdir:
            base_dir = Path(tmpdir)
            cmake_source = base_dir / 'cmake'
            cmake_source.mkdir()

            results = build_all_instances(
                instances=['test'],
                base_build_dir=base_dir,
                cmake_source=cmake_source,
                max_retries=1
            )

            assert results.successful_count == 1
            assert results.failed_count == 0
            assert results.successful_instances[0].compilation['total_time_s'] == 5.0
            assert mock_build.call_count == 1

    @patch('builder.build_instance')
    def test_multiple_instances_mixed_results(self, mock_build):
        """Test building multiple instances with mixed success/failure."""
        mock_build.side_effect = [
            # inst1: success on attempt 1
            BuildResult(success=True, instance_name='inst1', attempt=1,
                       timestamp='2025-01-14T15:00:00Z'),
            # inst2: fail attempt 1, success attempt 2
            BuildResult(success=False, instance_name='inst2', attempt=1,
                       timestamp='2025-01-14T15:00:05Z', errors=['Error 1']),
            BuildResult(success=True, instance_name='inst2', attempt=2,
                       timestamp='2025-01-14T15:00:10Z'),
            # inst3: fail both attempts
            BuildResult(success=False, instance_name='inst3', attempt=1,
                       timestamp='2025-01-14T15:00:15Z', errors=['Error 1']),
            BuildResult(success=False, instance_name='inst3', attempt=2,
                       timestamp='2025-01-14T15:00:20Z', errors=['Error 2']),
        ]

        with tempfile.TemporaryDirectory() as tmpdir:
            base_dir = Path(tmpdir)
            cmake_source = base_dir / 'cmake'
            cmake_source.mkdir()

            with patch('builder.time.sleep'):
                results = build_all_instances(
                    instances=['inst1', 'inst2', 'inst3'],
                    base_build_dir=base_dir,
                    cmake_source=cmake_source,
                    max_retries=2
                )

            assert results.successful_count == 2
            assert results.failed_count == 1
            assert results.total_instances == 3
            assert len(results.successful_instances) == 2
            assert len(results.failed_instances) == 1


class TestIntegrationWithBuildInstance:
    """Integration tests with build_instance function."""

    @patch('builder.subprocess.run')
    def test_error_message_captured(self, mock_run):
        """Test that error messages from build_instance are properly captured."""
        # Simulate CMake configuration failure
        mock_run.return_value = MagicMock(
            returncode=1,
            stderr='CMake Error: Could not find required package',
            stdout=''
        )

        with tempfile.TemporaryDirectory() as tmpdir:
            build_dir = Path(tmpdir) / 'build'
            cmake_source = Path(tmpdir) / 'cmake'
            cmake_source.mkdir()

            result = build_instance(
                instance_name='test',
                build_dir=build_dir,
                cmake_source=cmake_source,
                attempt=1
            )

            assert not result.success
            assert len(result.errors) > 0
            assert 'CMake' in result.errors[0]


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
