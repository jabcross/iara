"""
Unit tests for visualizer.py - Task 4.1 & 4.2: Vega-Lite Specification Translation & Validation.

Tests the main functions:
    Task 4.1:
        1. load_plot_specs() - Extract visualization.plots from YAML
        2. translate_plotly_to_vegalite() - Convert Plotly→Vega-Lite
        3. validate_vegalite_spec() - Validate output Vega-Lite JSON
    Task 4.2:
        1. inject_data_url() - Replace PLACEHOLDER with actual file path
        2. auto_select_display_unit() - Select appropriate unit based on magnitude
        3. add_unit_conversion_transform() - Add Vega-Lite transform for unit conversion
        4. apply_unit_conversions() - Apply conversions to all measurements in spec
        5. generate_vegalite_json() - Orchestrate complete workflow
"""

import json
import tempfile
import unittest
import logging
from pathlib import Path
from unittest.mock import patch, mock_open

from tools.experiment_framework.visualizer import (
    load_plot_specs,
    translate_plotly_to_vegalite,
    validate_vegalite_spec,
    inject_data_url,
    auto_select_display_unit,
    add_unit_conversion_transform,
    apply_unit_conversions,
    generate_vegalite_json,
    VALID_MARK_TYPES,
    VALID_ENCODING_CHANNELS,
)
from tools.experiment_framework.config import ValidationError


class TestLoadPlotSpecs(unittest.TestCase):
    """Tests for load_plot_specs function."""

    def test_load_valid_plots_from_yaml(self):
        """Test loading valid plots from YAML configuration."""
        yaml_config = {
            "visualization": {
                "plots": {
                    "runtime_performance": {
                        "title": "Runtime Performance",
                        "type": "grouped_bars",
                        "y_axis": {"metric": "wall_time"}
                    },
                    "memory_usage": {
                        "title": "Memory Usage",
                        "type": "grouped_bars",
                        "y_axis": {"metric": "max_rss_mb"}
                    }
                }
            }
        }

        result = load_plot_specs(yaml_config)

        self.assertEqual(len(result), 2)
        self.assertIn("runtime_performance", result)
        self.assertIn("memory_usage", result)
        self.assertEqual(result["runtime_performance"]["title"], "Runtime Performance")
        self.assertEqual(result["memory_usage"]["type"], "grouped_bars")

    def test_missing_visualization_section_returns_empty(self):
        """Test that missing visualization section returns empty dict."""
        yaml_config = {
            "application": {"name": "test_app"},
            "parameters": []
        }

        with self.assertLogs('tools.experiment_framework.visualizer', level='INFO') as log:
            result = load_plot_specs(yaml_config)

        self.assertEqual(result, {})
        self.assertTrue(any("No visualization section" in message for message in log.output))

    def test_missing_plots_returns_empty(self):
        """Test that missing plots section returns empty dict."""
        yaml_config = {
            "visualization": {
                "defaults": {
                    "backend": "plotly"
                }
            }
        }

        with self.assertLogs('tools.experiment_framework.visualizer', level='INFO') as log:
            result = load_plot_specs(yaml_config)

        self.assertEqual(result, {})
        self.assertTrue(any("No plots defined" in message for message in log.output))

    def test_empty_plots_dict_returns_empty(self):
        """Test that empty plots dict returns empty dict."""
        yaml_config = {
            "visualization": {
                "plots": {}
            }
        }

        with self.assertLogs('tools.experiment_framework.visualizer', level='INFO') as log:
            result = load_plot_specs(yaml_config)

        self.assertEqual(result, {})

    def test_multiple_plots_loaded(self):
        """Test loading multiple plots with different configurations."""
        yaml_config = {
            "visualization": {
                "plots": {
                    "plot1": {"title": "Plot 1", "type": "grouped_bars"},
                    "plot2": {"title": "Plot 2", "type": "stacked_bar"},
                    "plot3": {"title": "Plot 3", "type": "grouped_stacked_bars"}
                }
            }
        }

        result = load_plot_specs(yaml_config)

        self.assertEqual(len(result), 3)
        self.assertIn("plot1", result)
        self.assertIn("plot2", result)
        self.assertIn("plot3", result)

    def test_preserves_plot_structure(self):
        """Test that plot structure is preserved exactly."""
        yaml_config = {
            "visualization": {
                "plots": {
                    "complex_plot": {
                        "title": "Complex Plot",
                        "type": "grouped_stacked_bars",
                        "y_axis": {
                            "metric": "value",
                            "measurements": ["init_time", "compute_time"]
                        },
                        "x_axis": {
                            "matrix_size": "separate_plots",
                            "num_blocks": "group_by"
                        },
                        "scheduler": "ordered_within_group",
                        "stack_by": "measurement"
                    }
                }
            }
        }

        result = load_plot_specs(yaml_config)

        plot = result["complex_plot"]
        self.assertEqual(plot["title"], "Complex Plot")
        self.assertEqual(plot["type"], "grouped_stacked_bars")
        self.assertEqual(plot["y_axis"]["metric"], "value")
        self.assertEqual(len(plot["y_axis"]["measurements"]), 2)
        self.assertEqual(plot["x_axis"]["matrix_size"], "separate_plots")
        self.assertEqual(plot["x_axis"]["num_blocks"], "group_by")

    def test_invalid_plot_spec_structure_skipped(self):
        """Test that invalid plot specs are skipped with warning."""
        yaml_config = {
            "visualization": {
                "plots": {
                    "valid_plot": {"title": "Valid", "type": "grouped_bars"},
                    "invalid_plot": "not a dictionary",
                    "another_valid": {"title": "Another Valid"}
                }
            }
        }

        with self.assertLogs('tools.experiment_framework.visualizer', level='WARNING') as log:
            result = load_plot_specs(yaml_config)

        self.assertEqual(len(result), 2)
        self.assertIn("valid_plot", result)
        self.assertIn("another_valid", result)
        self.assertNotIn("invalid_plot", result)
        self.assertTrue(any("not a dictionary" in message for message in log.output))

    def test_plot_without_title_or_type_skipped(self):
        """Test that plots without title or type are skipped."""
        yaml_config = {
            "visualization": {
                "plots": {
                    "valid_plot": {"title": "Valid"},
                    "invalid_plot": {"description": "No title or type"},
                    "another_valid": {"type": "grouped_bars"}
                }
            }
        }

        with self.assertLogs('tools.experiment_framework.visualizer', level='WARNING') as log:
            result = load_plot_specs(yaml_config)

        self.assertEqual(len(result), 2)
        self.assertIn("valid_plot", result)
        self.assertIn("another_valid", result)
        self.assertNotIn("invalid_plot", result)


class TestTranslateToVegalite(unittest.TestCase):
    """Tests for translate_plotly_to_vegalite function."""

    def test_grouped_bars_translation(self):
        """Test translation of grouped_bars plot type."""
        plotly_spec = {
            "title": "Test Grouped Bars",
            "type": "grouped_bars",
            "y_axis": {"metric": "wall_time"},
            "x_axis": {"num_blocks": "group_by"},
            "scheduler": "ordered_within_group"
        }

        result = translate_plotly_to_vegalite("test_plot", plotly_spec)

        self.assertEqual(result["$schema"], "https://vega.github.io/schema/vega-lite/v5.json")
        self.assertEqual(result["title"], "Test Grouped Bars")
        self.assertEqual(result["mark"], "bar")
        self.assertIn("encoding", result)
        self.assertIn("x", result["encoding"])
        self.assertIn("y", result["encoding"])
        self.assertIn("color", result["encoding"])
        self.assertIn("xOffset", result["encoding"])
        self.assertEqual(result["encoding"]["x"]["field"], "parameters.num_blocks")
        self.assertEqual(result["encoding"]["y"]["field"], "execution.statistics.wall_time.mean")
        self.assertEqual(result["encoding"]["color"]["field"], "parameters.scheduler")

    def test_stacked_bar_translation(self):
        """Test translation of stacked_bar plot type."""
        plotly_spec = {
            "title": "Test Stacked Bar",
            "type": "stacked_bar",
            "y_axis": {"metric": "value"},
            "x_axis": {"num_blocks": "group_by"},
            "stack_by": "section"
        }

        result = translate_plotly_to_vegalite("test_plot", plotly_spec)

        self.assertEqual(result["title"], "Test Stacked Bar")
        self.assertEqual(result["mark"], "bar")
        self.assertEqual(result["encoding"]["x"]["field"], "parameters.num_blocks")
        self.assertEqual(result["encoding"]["y"]["field"], "execution.statistics.value.mean")
        self.assertEqual(result["encoding"]["color"]["field"], "parameters.section")

    def test_grouped_stacked_bars_translation(self):
        """Test translation of grouped_stacked_bars plot type."""
        plotly_spec = {
            "title": "Test Grouped Stacked Bars",
            "type": "grouped_stacked_bars",
            "y_axis": {
                "metric": "value",
                "measurements": ["init_time", "convert_time", "compute_time"]
            },
            "x_axis": {
                "matrix_size": "separate_plots",
                "num_blocks": "group_by"
            },
            "scheduler": "ordered_within_group",
            "stack_by": "measurement"
        }

        result = translate_plotly_to_vegalite("test_plot", plotly_spec)

        self.assertEqual(result["title"], "Test Grouped Stacked Bars")
        self.assertEqual(result["mark"], "bar")
        self.assertIn("column", result["encoding"])
        self.assertEqual(result["encoding"]["column"]["field"], "parameters.matrix_size")
        self.assertEqual(result["encoding"]["x"]["field"], "parameters.num_blocks")
        self.assertEqual(result["encoding"]["color"]["field"], "measurement_type")

    def test_unknown_plot_type_defaults_to_bars(self):
        """Test that unknown plot type defaults to grouped_bars."""
        plotly_spec = {
            "title": "Test Unknown Type",
            "type": "unknown_type",
            "y_axis": {"metric": "wall_time"},
            "x_axis": {"num_blocks": "group_by"}
        }

        with self.assertLogs('tools.experiment_framework.visualizer', level='WARNING') as log:
            result = translate_plotly_to_vegalite("test_plot", plotly_spec)

        self.assertEqual(result["mark"], "bar")
        self.assertIn("x", result["encoding"])
        self.assertIn("y", result["encoding"])
        self.assertTrue(any("Unknown plot type" in message for message in log.output))

    def test_preserves_title(self):
        """Test that title is preserved from plotly spec."""
        plotly_spec = {
            "title": "My Custom Title",
            "type": "grouped_bars",
            "y_axis": {"metric": "wall_time"}
        }

        result = translate_plotly_to_vegalite("test_plot", plotly_spec)

        self.assertEqual(result["title"], "My Custom Title")

    def test_adds_schema_url(self):
        """Test that Vega-Lite v5 schema URL is added."""
        plotly_spec = {
            "title": "Test",
            "type": "grouped_bars",
            "y_axis": {"metric": "wall_time"}
        }

        result = translate_plotly_to_vegalite("test_plot", plotly_spec)

        self.assertIn("$schema", result)
        self.assertIn("vega-lite/v5", result["$schema"])

    def test_creates_placeholder_data_url(self):
        """Test that data URL is set to PLACEHOLDER."""
        plotly_spec = {
            "title": "Test",
            "type": "grouped_bars",
            "y_axis": {"metric": "wall_time"}
        }

        result = translate_plotly_to_vegalite("test_plot", plotly_spec)

        self.assertEqual(result["data"]["url"], "PLACEHOLDER")
        self.assertEqual(result["data"]["format"]["type"], "json")
        self.assertEqual(result["data"]["format"]["property"], "instances")

    def test_sets_correct_encoding_types(self):
        """Test that encoding types are set correctly (ordinal/quantitative/nominal)."""
        plotly_spec = {
            "title": "Test",
            "type": "grouped_bars",
            "y_axis": {"metric": "wall_time"},
            "x_axis": {"num_blocks": "group_by"}
        }

        result = translate_plotly_to_vegalite("test_plot", plotly_spec)

        self.assertEqual(result["encoding"]["x"]["type"], "ordinal")
        self.assertEqual(result["encoding"]["y"]["type"], "quantitative")
        self.assertEqual(result["encoding"]["color"]["type"], "nominal")

    def test_handles_missing_title(self):
        """Test that plot_name is used as fallback when title is missing."""
        plotly_spec = {
            "type": "grouped_bars",
            "y_axis": {"metric": "wall_time"}
        }

        result = translate_plotly_to_vegalite("fallback_plot_name", plotly_spec)

        self.assertEqual(result["title"], "fallback_plot_name")

    def test_complex_x_axis_with_multiple_params(self):
        """Test x_axis with both separate_plots and group_by parameters."""
        plotly_spec = {
            "title": "Test",
            "type": "grouped_bars",
            "y_axis": {"metric": "wall_time"},
            "x_axis": {
                "matrix_size": "separate_plots",
                "num_blocks": "group_by"
            }
        }

        result = translate_plotly_to_vegalite("test_plot", plotly_spec)

        # Should have column faceting for separate_plots
        self.assertIn("column", result["encoding"])
        self.assertEqual(result["encoding"]["column"]["field"], "parameters.matrix_size")
        # Should have x encoding for group_by
        self.assertEqual(result["encoding"]["x"]["field"], "parameters.num_blocks")

    def test_missing_y_axis_raises_error(self):
        """Test that missing y_axis raises ValidationError."""
        plotly_spec = {
            "title": "Test",
            "type": "grouped_bars"
        }

        with self.assertRaises(ValidationError) as context:
            translate_plotly_to_vegalite("test_plot", plotly_spec)

        self.assertIn("y_axis", str(context.exception))

    def test_missing_metric_raises_error(self):
        """Test that missing metric in y_axis raises ValidationError."""
        plotly_spec = {
            "title": "Test",
            "type": "grouped_bars",
            "y_axis": {"measurements": ["time"]}  # Has y_axis but no metric
        }

        with self.assertRaises(ValidationError) as context:
            translate_plotly_to_vegalite("test_plot", plotly_spec)

        self.assertIn("metric", str(context.exception))

    def test_grouped_bars_without_group_param_uses_scheduler(self):
        """Test that grouped_bars without group_param uses scheduler for x."""
        plotly_spec = {
            "title": "Test",
            "type": "grouped_bars",
            "y_axis": {"metric": "wall_time"},
            "x_axis": {}
        }

        result = translate_plotly_to_vegalite("test_plot", plotly_spec)

        self.assertEqual(result["encoding"]["x"]["field"], "parameters.scheduler")
        self.assertNotIn("xOffset", result["encoding"])

    def test_faceting_only_without_grouping(self):
        """Test plot with faceting but no grouping parameter."""
        plotly_spec = {
            "title": "Test",
            "type": "grouped_bars",
            "y_axis": {"metric": "wall_time"},
            "x_axis": {
                "matrix_size": "separate_plots"
            }
        }

        result = translate_plotly_to_vegalite("test_plot", plotly_spec)

        self.assertIn("column", result["encoding"])
        self.assertEqual(result["encoding"]["column"]["field"], "parameters.matrix_size")
        # Should default to scheduler for x when no group_by
        self.assertEqual(result["encoding"]["x"]["field"], "parameters.scheduler")

    def test_title_formatting(self):
        """Test that field names are formatted correctly in titles."""
        plotly_spec = {
            "title": "Test",
            "type": "grouped_bars",
            "y_axis": {"metric": "max_rss_mb"},
            "x_axis": {"num_blocks": "group_by"}
        }

        result = translate_plotly_to_vegalite("test_plot", plotly_spec)

        self.assertEqual(result["encoding"]["x"]["title"], "Num Blocks")
        self.assertEqual(result["encoding"]["y"]["title"], "Max Rss Mb")

    def test_line_plot_translation(self):
        """Test translation of line_plot type."""
        plotly_spec = {
            "title": "Test Line Plot",
            "type": "line_plot",
            "y_axis": {"metric": "keypoints_found"},
            "x_axis": {"image_size": "x_axis"},
            "scheduler": "separate_lines"
        }

        result = translate_plotly_to_vegalite("test_plot", plotly_spec)

        self.assertEqual(result["mark"], "line")
        self.assertEqual(result["encoding"]["x"]["field"], "parameters.image_size")
        self.assertEqual(result["encoding"]["x"]["type"], "quantitative")
        self.assertEqual(result["encoding"]["y"]["field"], "execution.statistics.keypoints_found.mean")
        self.assertEqual(result["encoding"]["color"]["field"], "parameters.scheduler")


class TestValidateVegaliteSpec(unittest.TestCase):
    """Tests for validate_vegalite_spec function."""

    def test_valid_spec_passes(self):
        """Test that a valid spec passes validation."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "title": "Test Plot",
            "data": {"url": "data.json"},
            "mark": "bar",
            "encoding": {
                "x": {"field": "a", "type": "ordinal"},
                "y": {"field": "b", "type": "quantitative"}
            }
        }

        result = validate_vegalite_spec(spec)
        self.assertTrue(result)

    def test_missing_schema_fails(self):
        """Test that missing $schema raises ValidationError."""
        spec = {
            "data": {"url": "data.json"},
            "mark": "bar",
            "encoding": {"x": {}, "y": {}}
        }

        with self.assertRaises(ValidationError) as context:
            validate_vegalite_spec(spec)

        self.assertIn("$schema", str(context.exception))

    def test_wrong_schema_version_fails(self):
        """Test that wrong schema version raises ValidationError."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v4.json",
            "data": {"url": "data.json"},
            "mark": "bar",
            "encoding": {"x": {}, "y": {}}
        }

        with self.assertRaises(ValidationError) as context:
            validate_vegalite_spec(spec)

        self.assertIn("v5", str(context.exception))

    def test_non_vegalite_schema_fails(self):
        """Test that non-vega-lite schema raises ValidationError."""
        spec = {
            "$schema": "https://json-schema.org/draft-07/schema",
            "data": {"url": "data.json"},
            "mark": "bar",
            "encoding": {"x": {}, "y": {}}
        }

        with self.assertRaises(ValidationError) as context:
            validate_vegalite_spec(spec)

        self.assertIn("vega-lite", str(context.exception))

    def test_missing_data_fails(self):
        """Test that missing data field raises ValidationError."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "mark": "bar",
            "encoding": {"x": {}, "y": {}}
        }

        with self.assertRaises(ValidationError) as context:
            validate_vegalite_spec(spec)

        self.assertIn("data", str(context.exception))

    def test_missing_data_url_fails(self):
        """Test that missing data.url raises ValidationError."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {},
            "mark": "bar",
            "encoding": {"x": {}, "y": {}}
        }

        with self.assertRaises(ValidationError) as context:
            validate_vegalite_spec(spec)

        self.assertIn("url", str(context.exception))

    def test_invalid_mark_type_fails(self):
        """Test that invalid mark type raises ValidationError."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {"url": "data.json"},
            "mark": "invalid_mark",
            "encoding": {"x": {}, "y": {}}
        }

        with self.assertRaises(ValidationError) as context:
            validate_vegalite_spec(spec)

        self.assertIn("Invalid mark type", str(context.exception))
        self.assertIn("invalid_mark", str(context.exception))

    def test_missing_encoding_fails(self):
        """Test that missing encoding field raises ValidationError."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {"url": "data.json"},
            "mark": "bar"
        }

        with self.assertRaises(ValidationError) as context:
            validate_vegalite_spec(spec)

        self.assertIn("encoding", str(context.exception))

    def test_empty_encoding_fails(self):
        """Test that empty encoding dict raises ValidationError."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {"url": "data.json"},
            "mark": "bar",
            "encoding": {}
        }

        with self.assertRaises(ValidationError) as context:
            validate_vegalite_spec(spec)

        self.assertIn("at least one channel", str(context.exception))

    def test_unknown_encoding_channel_warns_not_fails(self):
        """Test that unknown encoding channel logs warning but passes."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {"url": "data.json"},
            "mark": "bar",
            "encoding": {
                "x": {"field": "a", "type": "ordinal"},
                "unknown_channel": {"field": "b"}
            }
        }

        with self.assertLogs('tools.experiment_framework.visualizer', level='WARNING') as log:
            result = validate_vegalite_spec(spec)

        self.assertTrue(result)
        self.assertTrue(any("Unknown encoding channel" in message for message in log.output))

    def test_all_valid_mark_types_pass(self):
        """Test that all valid mark types pass validation."""
        for mark_type in VALID_MARK_TYPES:
            spec = {
                "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
                "data": {"url": "data.json"},
                "mark": mark_type,
                "encoding": {"x": {}, "y": {}}
            }

            result = validate_vegalite_spec(spec)
            self.assertTrue(result, f"Mark type '{mark_type}' should be valid")

    def test_all_valid_encoding_channels_pass(self):
        """Test that all valid encoding channels pass validation."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {"url": "data.json"},
            "mark": "bar",
            "encoding": {}
        }

        # Add all valid channels
        for channel in VALID_ENCODING_CHANNELS:
            spec["encoding"][channel] = {"field": "test"}

        result = validate_vegalite_spec(spec)
        self.assertTrue(result)

    def test_mark_as_dict_with_type(self):
        """Test that mark as dict with type field is valid."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {"url": "data.json"},
            "mark": {"type": "bar", "color": "red"},
            "encoding": {"x": {}, "y": {}}
        }

        result = validate_vegalite_spec(spec)
        self.assertTrue(result)

    def test_mark_as_dict_without_type_fails(self):
        """Test that mark as dict without type field fails."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {"url": "data.json"},
            "mark": {"color": "red"},
            "encoding": {"x": {}, "y": {}}
        }

        with self.assertRaises(ValidationError) as context:
            validate_vegalite_spec(spec)

        self.assertIn("type", str(context.exception))

    def test_data_not_dict_fails(self):
        """Test that data as non-dict raises ValidationError."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": "not a dict",
            "mark": "bar",
            "encoding": {"x": {}, "y": {}}
        }

        with self.assertRaises(ValidationError) as context:
            validate_vegalite_spec(spec)

        self.assertIn("dictionary", str(context.exception))

    def test_encoding_not_dict_fails(self):
        """Test that encoding as non-dict raises ValidationError."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {"url": "data.json"},
            "mark": "bar",
            "encoding": "not a dict"
        }

        with self.assertRaises(ValidationError) as context:
            validate_vegalite_spec(spec)

        self.assertIn("dictionary", str(context.exception))

    def test_schema_not_string_fails(self):
        """Test that $schema as non-string raises ValidationError."""
        spec = {
            "$schema": 123,
            "data": {"url": "data.json"},
            "mark": "bar",
            "encoding": {"x": {}, "y": {}}
        }

        with self.assertRaises(ValidationError) as context:
            validate_vegalite_spec(spec)

        self.assertIn("string", str(context.exception))


class TestIntegration(unittest.TestCase):
    """Integration tests combining multiple functions."""

    def test_load_and_translate_cholesky_plots(self):
        """Test loading and translating actual Cholesky experiment plots."""
        yaml_config = {
            "visualization": {
                "plots": {
                    "runtime_performance": {
                        "title": "Cholesky Runtime Performance",
                        "type": "grouped_stacked_bars",
                        "y_axis": {
                            "metric": "value",
                            "measurements": ["init_time", "convert_time", "compute_time"]
                        },
                        "x_axis": {
                            "matrix_size": "separate_plots",
                            "num_blocks": "group_by"
                        },
                        "scheduler": "ordered_within_group",
                        "stack_by": "measurement"
                    },
                    "memory_usage": {
                        "title": "Peak Memory Usage",
                        "type": "grouped_bars",
                        "y_axis": {"metric": "max_rss_mb"},
                        "x_axis": {
                            "matrix_size": "separate_plots",
                            "num_blocks": "group_by"
                        },
                        "scheduler": "ordered_within_group"
                    }
                }
            }
        }

        # Load plots
        plots = load_plot_specs(yaml_config)
        self.assertEqual(len(plots), 2)

        # Translate each plot
        for plot_name, plot_spec in plots.items():
            vl_spec = translate_plotly_to_vegalite(plot_name, plot_spec)

            # Validate result
            self.assertTrue(validate_vegalite_spec(vl_spec))
            self.assertEqual(vl_spec["data"]["url"], "PLACEHOLDER")
            self.assertIn("$schema", vl_spec)
            self.assertIn("v5", vl_spec["$schema"])

    def test_load_and_translate_sift_plots(self):
        """Test loading and translating SIFT experiment plots."""
        yaml_config = {
            "visualization": {
                "plots": {
                    "runtime_performance": {
                        "title": "SIFT Runtime Performance",
                        "type": "grouped_bars",
                        "y_axis": {"metric": "wall_time"},
                        "x_axis": {"image_size": "separate_plots"},
                        "scheduler": "ordered_within_group"
                    }
                }
            }
        }

        plots = load_plot_specs(yaml_config)
        self.assertEqual(len(plots), 1)

        vl_spec = translate_plotly_to_vegalite("runtime_performance", plots["runtime_performance"])
        self.assertTrue(validate_vegalite_spec(vl_spec))
        self.assertEqual(vl_spec["title"], "SIFT Runtime Performance")

    def test_end_to_end_workflow(self):
        """Test complete workflow from YAML to validated Vega-Lite spec."""
        yaml_config = {
            "visualization": {
                "plots": {
                    "test_plot": {
                        "title": "Test Performance",
                        "type": "grouped_bars",
                        "y_axis": {"metric": "wall_time"},
                        "x_axis": {"size": "group_by"}
                    }
                }
            }
        }

        # Step 1: Load plot specs
        plots = load_plot_specs(yaml_config)
        self.assertIn("test_plot", plots)

        # Step 2: Translate to Vega-Lite
        vl_spec = translate_plotly_to_vegalite("test_plot", plots["test_plot"])

        # Step 3: Validate
        is_valid = validate_vegalite_spec(vl_spec)
        self.assertTrue(is_valid)

        # Verify structure
        self.assertEqual(vl_spec["title"], "Test Performance")
        self.assertEqual(vl_spec["mark"], "bar")
        self.assertEqual(vl_spec["data"]["url"], "PLACEHOLDER")
        self.assertEqual(vl_spec["encoding"]["x"]["field"], "parameters.size")
        self.assertEqual(vl_spec["encoding"]["y"]["field"], "execution.statistics.wall_time.mean")


class TestInjectDataUrl(unittest.TestCase):
    """Tests for inject_data_url function (Task 4.2)."""

    def test_inject_absolute_path_creates_file_url(self):
        """Test that absolute path is converted to file:// URL."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {"url": "PLACEHOLDER"},
            "mark": "bar",
            "encoding": {}
        }

        with tempfile.NamedTemporaryFile(suffix='.json', delete=False) as f:
            temp_path = Path(f.name)

        try:
            result = inject_data_url(spec, temp_path)

            self.assertIn("file://", result["data"]["url"])
            self.assertIn(str(temp_path.resolve()), result["data"]["url"])
            self.assertEqual(result["data"]["format"]["type"], "json")
            self.assertEqual(result["data"]["format"]["property"], "instances")
        finally:
            temp_path.unlink()

    def test_inject_relative_path_resolved(self):
        """Test that relative paths are converted to absolute."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {"url": "PLACEHOLDER"},
            "mark": "bar",
            "encoding": {}
        }

        # Use a relative path
        relative_path = Path("./test_results.json")

        result = inject_data_url(spec, relative_path)

        # Should be converted to absolute path
        self.assertIn("file://", result["data"]["url"])
        self.assertTrue(result["data"]["url"].startswith("file:///"))

    def test_log_warning_for_missing_file(self):
        """Test that missing file logs warning but doesn't fail."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {"url": "PLACEHOLDER"},
            "mark": "bar",
            "encoding": {}
        }

        nonexistent_path = Path("/nonexistent/path/results.json")

        with self.assertLogs('tools.experiment_framework.visualizer', level='WARNING') as log:
            result = inject_data_url(spec, nonexistent_path)

        self.assertIn("file://", result["data"]["url"])
        self.assertTrue(any("does not exist yet" in message for message in log.output))

    def test_preserve_spec_structure(self):
        """Test that original spec structure is preserved."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "title": "Test Plot",
            "data": {"url": "PLACEHOLDER"},
            "mark": "bar",
            "encoding": {
                "x": {"field": "a", "type": "ordinal"},
                "y": {"field": "b", "type": "quantitative"}
            }
        }

        temp_path = Path("/tmp/test_results.json")
        result = inject_data_url(spec, temp_path)

        # Check structure is preserved
        self.assertEqual(result["$schema"], spec["$schema"])
        self.assertEqual(result["title"], spec["title"])
        self.assertEqual(result["mark"], spec["mark"])
        self.assertEqual(result["encoding"], spec["encoding"])

    def test_set_data_format_correct(self):
        """Test that data format is set correctly."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {"url": "PLACEHOLDER"},
            "mark": "bar",
            "encoding": {}
        }

        temp_path = Path("/tmp/results.json")
        result = inject_data_url(spec, temp_path)

        self.assertEqual(result["data"]["format"]["type"], "json")
        self.assertEqual(result["data"]["format"]["property"], "instances")

    def test_invalid_path_raises_error(self):
        """Test that None path raises ValueError."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {"url": "PLACEHOLDER"},
            "mark": "bar",
            "encoding": {}
        }

        with self.assertRaises(ValueError):
            inject_data_url(spec, None)

    def test_original_spec_not_modified(self):
        """Test that original spec is not modified (deep copy)."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {"url": "PLACEHOLDER"},
            "mark": "bar",
            "encoding": {}
        }

        original_url = spec["data"]["url"]
        temp_path = Path("/tmp/results.json")

        result = inject_data_url(spec, temp_path)

        # Original should be unchanged
        self.assertEqual(spec["data"]["url"], original_url)
        # Result should be modified
        self.assertNotEqual(result["data"]["url"], original_url)


class TestAutoSelectDisplayUnit(unittest.TestCase):
    """Tests for auto_select_display_unit function (Task 4.2)."""

    def test_time_microseconds_small_values(self):
        """Test microsecond selection for very small time values."""
        unit, scale = auto_select_display_unit("compute_time_s", [0.0005, 0.0008])
        self.assertEqual(unit, "μs")
        self.assertEqual(scale, 1e6)

    def test_time_milliseconds_medium_values(self):
        """Test millisecond selection for medium time values."""
        unit, scale = auto_select_display_unit("wall_time_s", [0.1, 0.5, 0.8])
        self.assertEqual(unit, "ms")
        self.assertEqual(scale, 1000)

    def test_time_seconds_large_values(self):
        """Test second selection for large time values."""
        unit, scale = auto_select_display_unit("elapsed_time_s", [1.5, 2.0, 5.5])
        self.assertEqual(unit, "s")
        self.assertEqual(scale, 1.0)

    def test_memory_bytes_very_small(self):
        """Test bytes selection for very small memory values."""
        unit, scale = auto_select_display_unit("rss_bytes", [512, 1000])
        self.assertEqual(unit, "B")
        self.assertEqual(scale, 1)

    def test_memory_kb_small_values(self):
        """Test KB selection for small memory values."""
        unit, scale = auto_select_display_unit("max_rss_bytes", [2048, 10240])
        self.assertEqual(unit, "KB")
        self.assertEqual(scale, 1024)

    def test_memory_mb_medium_values(self):
        """Test MB selection for medium memory values."""
        unit, scale = auto_select_display_unit("memory_usage_bytes", [2048000, 3072000])
        self.assertEqual(unit, "MB")
        self.assertEqual(scale, 1024**2)

    def test_memory_gb_large_values(self):
        """Test GB selection for large memory values."""
        unit, scale = auto_select_display_unit("peak_memory_bytes", [2147483648, 3221225472])
        self.assertEqual(unit, "GB")
        self.assertEqual(scale, 1024**3)

    def test_memory_tb_very_large_values(self):
        """Test TB selection for very large memory values."""
        unit, scale = auto_select_display_unit("max_rss_bytes", [1099511627776, 2199023255552])
        self.assertEqual(unit, "TB")
        self.assertEqual(scale, 1024**4)

    def test_empty_values_returns_base_unit(self):
        """Test that empty values return base unit."""
        unit, scale = auto_select_display_unit("compute_time_s", [])
        self.assertEqual(unit, "μs")
        self.assertEqual(scale, 1e6)

    def test_unknown_measurement_returns_no_conversion(self):
        """Test that unknown measurement types return no conversion."""
        unit, scale = auto_select_display_unit("keypoints_found", [100, 200, 300])
        self.assertEqual(unit, "")
        self.assertEqual(scale, 1.0)


class TestAddUnitConversionTransform(unittest.TestCase):
    """Tests for add_unit_conversion_transform function (Task 4.2)."""

    def test_add_to_empty_transforms(self):
        """Test adding transform to spec without existing transforms."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {"url": "test.json"},
            "mark": "bar",
            "encoding": {
                "y": {
                    "field": "execution.statistics.compute_time_s.mean",
                    "type": "quantitative",
                    "title": "Compute Time"
                }
            }
        }

        result = add_unit_conversion_transform(spec, "compute_time_s", "ms", 1000)

        self.assertIn("transform", result)
        self.assertEqual(len(result["transform"]), 1)
        self.assertEqual(
            result["transform"][0]["calculate"],
            "datum['execution.statistics.compute_time_s.mean'] / 1000"
        )
        self.assertEqual(result["transform"][0]["as"], "compute_time_s_display")

    def test_append_to_existing_transforms(self):
        """Test appending to existing transforms list."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {"url": "test.json"},
            "mark": "bar",
            "transform": [
                {"filter": "datum.x > 0"}
            ],
            "encoding": {
                "y": {
                    "field": "execution.statistics.max_rss_bytes.mean",
                    "type": "quantitative",
                    "title": "Memory"
                }
            }
        }

        result = add_unit_conversion_transform(spec, "max_rss_bytes", "MB", 1024**2)

        self.assertEqual(len(result["transform"]), 2)
        self.assertEqual(result["transform"][0]["filter"], "datum.x > 0")
        self.assertEqual(result["transform"][1]["as"], "max_rss_bytes_display")

    def test_update_encoding_field_reference(self):
        """Test that encoding field reference is updated correctly."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {"url": "test.json"},
            "mark": "bar",
            "encoding": {
                "y": {
                    "field": "execution.statistics.wall_time_s.mean",
                    "type": "quantitative",
                    "title": "Wall Time"
                }
            }
        }

        result = add_unit_conversion_transform(spec, "wall_time_s", "ms", 1000)

        self.assertEqual(result["encoding"]["y"]["field"], "wall_time_s_display")

    def test_update_axis_title_with_unit(self):
        """Test that axis title is updated with unit."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {"url": "test.json"},
            "mark": "bar",
            "encoding": {
                "y": {
                    "field": "execution.statistics.compute_time_s.mean",
                    "type": "quantitative",
                    "title": "Compute Time"
                }
            }
        }

        result = add_unit_conversion_transform(spec, "compute_time_s", "ms", 1000)

        self.assertEqual(result["encoding"]["y"]["title"], "Compute Time (ms)")

    def test_correct_scale_factor_applied(self):
        """Test that correct scale factor is used in transform."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {"url": "test.json"},
            "mark": "bar",
            "encoding": {
                "y": {
                    "field": "execution.statistics.max_rss_bytes.mean",
                    "type": "quantitative",
                    "title": "Memory"
                }
            }
        }

        result = add_unit_conversion_transform(spec, "max_rss_bytes", "MB", 1048576)

        self.assertIn("1048576", result["transform"][0]["calculate"])

    def test_skip_conversion_when_no_unit(self):
        """Test that conversion is skipped when scale is 1.0 and no unit."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {"url": "test.json"},
            "mark": "bar",
            "encoding": {
                "y": {
                    "field": "execution.statistics.keypoints_found.mean",
                    "type": "quantitative",
                    "title": "Keypoints"
                }
            }
        }

        result = add_unit_conversion_transform(spec, "keypoints_found", "", 1.0)

        # Should not add transform
        self.assertNotIn("transform", result)
        # Encoding should be unchanged
        self.assertEqual(result["encoding"]["y"]["field"], "execution.statistics.keypoints_found.mean")


class TestApplyUnitConversions(unittest.TestCase):
    """Tests for apply_unit_conversions function (Task 4.2)."""

    def test_apply_time_conversions(self):
        """Test applying conversions for time measurements."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {"url": "test.json"},
            "mark": "bar",
            "encoding": {
                "y": {
                    "field": "execution.statistics.compute_time_s.mean",
                    "type": "quantitative",
                    "title": "Compute Time"
                }
            }
        }

        results_data = {
            "instances": [
                {"execution": {"statistics": {"compute_time_s": {"mean": 0.5}}}},
                {"execution": {"statistics": {"compute_time_s": {"mean": 0.8}}}},
            ]
        }

        result = apply_unit_conversions(spec, results_data)

        # Should add transform for milliseconds
        self.assertIn("transform", result)
        self.assertEqual(result["encoding"]["y"]["field"], "compute_time_s_display")
        self.assertIn("(ms)", result["encoding"]["y"]["title"])

    def test_apply_memory_conversions(self):
        """Test applying conversions for memory measurements."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {"url": "test.json"},
            "mark": "bar",
            "encoding": {
                "y": {
                    "field": "execution.statistics.max_rss_bytes.mean",
                    "type": "quantitative",
                    "title": "Memory"
                }
            }
        }

        results_data = {
            "instances": [
                {"execution": {"statistics": {"max_rss_bytes": {"mean": 2048000}}}},
                {"execution": {"statistics": {"max_rss_bytes": {"mean": 3072000}}}},
            ]
        }

        result = apply_unit_conversions(spec, results_data)

        # Should add transform for megabytes
        self.assertIn("transform", result)
        self.assertEqual(result["encoding"]["y"]["field"], "max_rss_bytes_display")
        self.assertIn("(MB)", result["encoding"]["y"]["title"])

    def test_handle_missing_data_gracefully(self):
        """Test handling of missing data gracefully."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {"url": "test.json"},
            "mark": "bar",
            "encoding": {
                "y": {
                    "field": "execution.statistics.compute_time_s.mean",
                    "type": "quantitative",
                    "title": "Compute Time"
                }
            }
        }

        results_data = {"instances": []}

        with self.assertLogs('tools.experiment_framework.visualizer', level='WARNING') as log:
            result = apply_unit_conversions(spec, results_data)

        # Should return spec unchanged
        self.assertNotIn("transform", result)
        self.assertTrue(any("No instances data" in message for message in log.output))

    def test_multiple_measurements_single_spec(self):
        """Test handling multiple measurements in a single spec."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {"url": "test.json"},
            "mark": "bar",
            "encoding": {
                "y": {
                    "field": "execution.statistics.compute_time_s.mean",
                    "type": "quantitative",
                    "title": "Time"
                },
                "size": {
                    "field": "execution.statistics.max_rss_bytes.mean",
                    "type": "quantitative",
                    "title": "Memory"
                }
            }
        }

        results_data = {
            "instances": [
                {
                    "execution": {
                        "statistics": {
                            "compute_time_s": {"mean": 0.5},
                            "max_rss_bytes": {"mean": 2048000}
                        }
                    }
                }
            ]
        }

        result = apply_unit_conversions(spec, results_data)

        # Should add transforms for both measurements
        self.assertEqual(len(result["transform"]), 2)
        self.assertEqual(result["encoding"]["y"]["field"], "compute_time_s_display")
        self.assertEqual(result["encoding"]["size"]["field"], "max_rss_bytes_display")

    def test_log_warnings_for_missing_measurements(self):
        """Test that warnings are logged for missing measurements."""
        spec = {
            "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
            "data": {"url": "test.json"},
            "mark": "bar",
            "encoding": {
                "y": {
                    "field": "execution.statistics.missing_metric.mean",
                    "type": "quantitative",
                    "title": "Missing"
                }
            }
        }

        results_data = {
            "instances": [
                {"execution": {"statistics": {"compute_time_s": {"mean": 0.5}}}}
            ]
        }

        with self.assertLogs('tools.experiment_framework.visualizer', level='WARNING') as log:
            result = apply_unit_conversions(spec, results_data)

        self.assertTrue(any("No values found" in message for message in log.output))


class TestGenerateVegaliteJson(unittest.TestCase):
    """Tests for generate_vegalite_json function (Task 4.2)."""

    def test_generate_multiple_plots(self):
        """Test generating multiple plot files successfully."""
        yaml_config = {
            "visualization": {
                "plots": {
                    "plot1": {
                        "title": "Plot 1",
                        "type": "grouped_bars",
                        "y_axis": {"metric": "wall_time_s"}
                    },
                    "plot2": {
                        "title": "Plot 2",
                        "type": "grouped_bars",
                        "y_axis": {"metric": "max_rss_bytes"}
                    }
                }
            }
        }

        results_data = {
            "experiment": {"timestamp": "2024-01-15T10-00-00"},
            "instances": [
                {
                    "execution": {
                        "statistics": {
                            "wall_time_s": {"mean": 0.5},
                            "max_rss_bytes": {"mean": 2048000}
                        }
                    }
                }
            ]
        }

        with tempfile.TemporaryDirectory() as tmpdir:
            output_dir = Path(tmpdir) / "output"
            results_file = Path(tmpdir) / "results.json"

            # Write results file
            with open(results_file, 'w') as f:
                json.dump(results_data, f)

            # Generate visualizations
            generated = generate_vegalite_json(yaml_config, results_file, output_dir)

            # Check outputs
            self.assertEqual(len(generated), 2)
            self.assertTrue(all(f.exists() for f in generated))
            self.assertTrue(all(str(f).endswith('.vl.json') for f in generated))

    def test_skip_invalid_plot_continue_others(self):
        """Test that invalid plot is skipped but others continue."""
        yaml_config = {
            "visualization": {
                "plots": {
                    "valid_plot": {
                        "title": "Valid",
                        "type": "grouped_bars",
                        "y_axis": {"metric": "wall_time_s"}
                    },
                    "invalid_plot": {
                        "title": "Invalid",
                        "type": "grouped_bars"
                        # Missing required y_axis
                    },
                    "another_valid": {
                        "title": "Another Valid",
                        "type": "grouped_bars",
                        "y_axis": {"metric": "compute_time_s"}
                    }
                }
            }
        }

        results_data = {
            "experiment": {"timestamp": "2024-01-15T10-00-00"},
            "instances": []
        }

        with tempfile.TemporaryDirectory() as tmpdir:
            output_dir = Path(tmpdir) / "output"
            results_file = Path(tmpdir) / "results.json"

            with open(results_file, 'w') as f:
                json.dump(results_data, f)

            with self.assertLogs('tools.experiment_framework.visualizer', level='WARNING') as log:
                generated = generate_vegalite_json(yaml_config, results_file, output_dir)

            # Should generate 2 valid plots
            self.assertEqual(len(generated), 2)
            self.assertTrue(any("Skipping plot" in message for message in log.output))

    def test_create_output_directory(self):
        """Test that output directory is created if missing."""
        yaml_config = {
            "visualization": {
                "plots": {
                    "test_plot": {
                        "title": "Test",
                        "type": "grouped_bars",
                        "y_axis": {"metric": "wall_time_s"}
                    }
                }
            }
        }

        results_data = {
            "experiment": {"timestamp": "2024-01-15T10-00-00"},
            "instances": []
        }

        with tempfile.TemporaryDirectory() as tmpdir:
            output_dir = Path(tmpdir) / "nested" / "output" / "dir"
            results_file = Path(tmpdir) / "results.json"

            self.assertFalse(output_dir.exists())

            with open(results_file, 'w') as f:
                json.dump(results_data, f)

            generated = generate_vegalite_json(yaml_config, results_file, output_dir)

            # Directory should be created
            self.assertTrue(output_dir.exists())
            self.assertTrue(output_dir.is_dir())

    def test_correct_timestamp_in_filenames(self):
        """Test that timestamp from results is used in filenames."""
        yaml_config = {
            "visualization": {
                "plots": {
                    "test_plot": {
                        "title": "Test",
                        "type": "grouped_bars",
                        "y_axis": {"metric": "wall_time_s"}
                    }
                }
            }
        }

        results_data = {
            "experiment": {"timestamp": "2024-01-15T10-30-45"},
            "instances": []
        }

        with tempfile.TemporaryDirectory() as tmpdir:
            output_dir = Path(tmpdir) / "output"
            results_file = Path(tmpdir) / "results.json"

            with open(results_file, 'w') as f:
                json.dump(results_data, f)

            generated = generate_vegalite_json(yaml_config, results_file, output_dir)

            # Check filename includes timestamp
            self.assertEqual(len(generated), 1)
            self.assertIn("2024-01-15T10-30-45", str(generated[0]))

    def test_output_files_valid_json(self):
        """Test that output files contain valid JSON."""
        yaml_config = {
            "visualization": {
                "plots": {
                    "test_plot": {
                        "title": "Test",
                        "type": "grouped_bars",
                        "y_axis": {"metric": "wall_time_s"}
                    }
                }
            }
        }

        results_data = {
            "experiment": {"timestamp": "2024-01-15T10-00-00"},
            "instances": []
        }

        with tempfile.TemporaryDirectory() as tmpdir:
            output_dir = Path(tmpdir) / "output"
            results_file = Path(tmpdir) / "results.json"

            with open(results_file, 'w') as f:
                json.dump(results_data, f)

            generated = generate_vegalite_json(yaml_config, results_file, output_dir)

            # Try to load each file as JSON
            for file_path in generated:
                with open(file_path, 'r') as f:
                    data = json.load(f)
                    self.assertIsInstance(data, dict)

    def test_output_vegalite_specs_usable(self):
        """Test that output specs are valid Vega-Lite."""
        yaml_config = {
            "visualization": {
                "plots": {
                    "test_plot": {
                        "title": "Test Plot",
                        "type": "grouped_bars",
                        "y_axis": {"metric": "wall_time_s"},
                        "x_axis": {"num_blocks": "group_by"}
                    }
                }
            }
        }

        results_data = {
            "experiment": {"timestamp": "2024-01-15T10-00-00"},
            "instances": [
                {
                    "execution": {
                        "statistics": {
                            "wall_time_s": {"mean": 0.5}
                        }
                    }
                }
            ]
        }

        with tempfile.TemporaryDirectory() as tmpdir:
            output_dir = Path(tmpdir) / "output"
            results_file = Path(tmpdir) / "results.json"

            with open(results_file, 'w') as f:
                json.dump(results_data, f)

            generated = generate_vegalite_json(yaml_config, results_file, output_dir)

            # Load and validate spec
            with open(generated[0], 'r') as f:
                spec = json.load(f)

            # Check Vega-Lite structure
            self.assertIn("$schema", spec)
            self.assertIn("vega-lite/v5", spec["$schema"])
            self.assertIn("data", spec)
            self.assertIn("mark", spec)
            self.assertIn("encoding", spec)
            self.assertIn("file://", spec["data"]["url"])

    def test_no_plots_returns_empty(self):
        """Test that no plots in config returns empty list."""
        yaml_config = {
            "visualization": {}
        }

        with tempfile.TemporaryDirectory() as tmpdir:
            output_dir = Path(tmpdir) / "output"
            results_file = Path(tmpdir) / "results.json"

            results_file.write_text("{}")

            generated = generate_vegalite_json(yaml_config, results_file, output_dir)

            self.assertEqual(generated, [])


if __name__ == '__main__':
    unittest.main()
