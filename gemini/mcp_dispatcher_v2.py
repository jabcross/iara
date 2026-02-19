#!/usr/bin/env python3
"""
Simplified MCP Server Dispatcher for GPU Worker

Uses the standard MCP stdio transport wrapped in an HTTP server.
"""

import asyncio
import json
from typing import Any

import httpx
from mcp.server import Server
from mcp.server.stdio import stdio_server
from mcp.types import Tool, TextContent

# --- Configuration ---
DESKTOP_4090_IP = "enqii"
OLLAMA_ENDPOINT = f"http://{DESKTOP_4090_IP}:11434/api/generate"

# --- MCP Server Instance ---
app = Server("gpu-worker")


@app.list_tools()
async def list_tools() -> list[Tool]:
    """List available tools."""
    return [
        Tool(
            name="implement_code",
            description="Generate code implementation based on instructions and context",
            inputSchema={
                "type": "object",
                "properties": {
                    "instruction": {
                        "type": "string",
                        "description": "What code to implement"
                    },
                    "code_context": {
                        "type": "string",
                        "description": "Relevant code context"
                    }
                },
                "required": ["instruction"]
            }
        )
    ]


@app.call_tool()
async def call_tool(name: str, arguments: Any) -> list[TextContent]:
    """Handle tool invocations."""
    if name != "implement_code":
        raise ValueError(f"Unknown tool: {name}")

    instruction = arguments.get("instruction", "")
    code_context = arguments.get("code_context", "")

    prompt = f"""You are an expert programmer. Implement the requested code.

RULES:
- Output ONLY raw code
- NO markdown code blocks
- NO explanations
- Make it production-ready

Instruction:
{instruction}

Context:
{code_context}

Code:"""

    payload = {
        "model": "deepseek-coder-v2",
        "prompt": prompt,
        "stream": False,
        "options": {"temperature": 0.2}
    }

    try:
        async with httpx.AsyncClient(timeout=300.0) as client:
            response = await client.post(OLLAMA_ENDPOINT, json=payload)
            response.raise_for_status()
            result = response.json()
            code = result.get("response", "").strip()

            # Clean markdown artifacts
            if code.startswith("```"):
                lines = code.split("\n")
                if lines[0].startswith("```"):
                    lines = lines[1:]
                if lines and lines[-1].strip() == "```":
                    lines = lines[:-1]
                code = "\n".join(lines)

            return [TextContent(type="text", text=code)]

    except Exception as e:
        return [TextContent(type="text", text=f"ERROR: {e}")]


async def main():
    """Run the MCP server using stdio transport."""
    print("Starting MCP GPU Worker (stdio mode)", flush=True)
    print(f"Forwarding to: {OLLAMA_ENDPOINT}", flush=True)

    async with stdio_server() as (read_stream, write_stream):
        await app.run(
            read_stream,
            write_stream,
            app.create_initialization_options()
        )


if __name__ == "__main__":
    asyncio.run(main())
