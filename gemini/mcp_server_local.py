#!/usr/bin/env python3
"""
Simple MCP Server running locally on HPC server
This version uses stdio transport which Claude Code can connect to directly.
"""

import sys
import json
import asyncio
import httpx
from typing import Any

# Configuration
OLLAMA_ENDPOINT = "http://enqii:11434/api/generate"

async def call_ollama(instruction: str, code_context: str = "") -> str:
    """Call Ollama to generate code."""
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

            return code

    except Exception as e:
        return f"ERROR: {e}"


async def handle_message(message: dict) -> dict:
    """Handle MCP protocol messages."""
    method = message.get("method")
    params = message.get("params", {})
    msg_id = message.get("id")

    if method == "initialize":
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": {
                "protocolVersion": "2024-11-05",
                "capabilities": {
                    "tools": {}
                },
                "serverInfo": {
                    "name": "GPU Worker",
                    "version": "1.0.0"
                }
            }
        }

    elif method == "tools/list":
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": {
                "tools": [
                    {
                        "name": "implement_code",
                        "description": "Generate code implementation based on instructions and context",
                        "inputSchema": {
                            "type": "object",
                            "properties": {
                                "instruction": {
                                    "type": "string",
                                    "description": "What code to implement"
                                },
                                "code_context": {
                                    "type": "string",
                                    "description": "Relevant code context",
                                    "default": ""
                                }
                            },
                            "required": ["instruction"]
                        }
                    }
                ]
            }
        }

    elif method == "tools/call":
        tool_name = params.get("name")
        tool_params = params.get("arguments", {})

        if tool_name == "implement_code":
            instruction = tool_params.get("instruction", "")
            code_context = tool_params.get("code_context", "")

            print(f"DEBUG: Calling implement_code with instruction: {instruction[:100]}...", file=sys.stderr)

            code = await call_ollama(instruction, code_context)

            return {
                "jsonrpc": "2.0",
                "id": msg_id,
                "result": {
                    "content": [
                        {
                            "type": "text",
                            "text": code
                        }
                    ]
                }
            }
        else:
            return {
                "jsonrpc": "2.0",
                "id": msg_id,
                "error": {
                    "code": -32601,
                    "message": f"Method not found: {tool_name}"
                }
            }

    else:
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "error": {
                "code": -32601,
                "message": f"Method not found: {method}"
            }
        }


async def main():
    """Main server loop - read JSON from stdin, write to stdout."""
    print("DEBUG: MCP Server started, reading from stdin...", file=sys.stderr)

    loop = asyncio.get_event_loop()

    while True:
        try:
            # Read a line from stdin
            line = await loop.run_in_executor(None, sys.stdin.readline)

            if not line:
                break

            line = line.strip()
            if not line:
                continue

            # Parse JSON
            message = json.loads(line)
            print(f"DEBUG: Received message: {message.get('method')}", file=sys.stderr)

            # Handle message
            response = await handle_message(message)

            # Send response
            print(json.dumps(response))
            sys.stdout.flush()

        except json.JSONDecodeError as e:
            print(f"DEBUG: JSON parse error: {e}", file=sys.stderr)
            response = {
                "jsonrpc": "2.0",
                "id": None,
                "error": {
                    "code": -32700,
                    "message": "Parse error"
                }
            }
            print(json.dumps(response))
            sys.stdout.flush()
        except Exception as e:
            print(f"DEBUG: Error: {e}", file=sys.stderr)
            break


if __name__ == "__main__":
    asyncio.run(main())
