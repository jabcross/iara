#!/usr/bin/env python3
"""
MCP Server Dispatcher for GPU Worker - Simple HTTP JSON-RPC Handler

This script handles JSON-RPC 2.0 requests and forwards code generation requests
to an Ollama instance running on a GPU desktop.
"""

import asyncio
import json
from typing import Any

import httpx
import uvicorn
from starlette.applications import Starlette
from starlette.routing import Route
from starlette.requests import Request
from starlette.responses import JSONResponse

# --- Configuration ---
DESKTOP_4090_IP = "10.68.110.22"  # Your GPU desktop hostname/IP
OLLAMA_ENDPOINT = f"http://{DESKTOP_4090_IP}:11434/api/generate"
LAPTOP_HOST = "localhost"
LAPTOP_PORT = 9090


async def handle_mcp_request(request: Request):
    """Handle JSON-RPC 2.0 MCP requests."""
    try:
        message = await request.json()

        method = message.get("method")
        params = message.get("params", {})
        msg_id = message.get("id")

        # Handle tools/list request
        if method == "tools/list":
            return JSONResponse({
                "jsonrpc": "2.0",
                "id": msg_id,
                "result": {
                    "tools": [
                        {
                            "name": "implement_code",
                            "description": "Generate code implementation based on instructions and context. Returns raw code without markdown formatting or explanations.",
                            "inputSchema": {
                                "type": "object",
                                "properties": {
                                    "instruction": {
                                        "type": "string",
                                        "description": "Clear description of what code to implement"
                                    },
                                    "code_context": {
                                        "type": "string",
                                        "description": "Relevant code context (existing code, file contents, etc.)"
                                    }
                                },
                                "required": ["instruction"]
                            }
                        }
                    ]
                }
            })

        # Handle tools/call request
        elif method == "tools/call":
            tool_name = params.get("name")
            tool_args = params.get("arguments", {})

            if tool_name != "implement_code":
                return JSONResponse({
                    "jsonrpc": "2.0",
                    "id": msg_id,
                    "error": {
                        "code": -32601,
                        "message": f"Unknown tool: {tool_name}"
                    }
                })

            instruction = tool_args.get("instruction", "")
            code_context = tool_args.get("code_context", "")

            # Build prompt for Ollama
            prompt = f"""You are an expert programmer. Implement the requested code based on the instruction and context provided.

CRITICAL RULES:
- Output ONLY the raw code implementation
- Do NOT include markdown code blocks (no ```)
- Do NOT include explanations or comments outside the code
- Do NOT include conversational text
- Make the code production-ready and well-structured

Instruction:
{instruction}

Code Context:
{code_context}

Implementation:"""

            # Prepare Ollama request payload
            ollama_payload = {
                "model": "deepseek-coder-v2",
                "prompt": prompt,
                "stream": False,
                "options": {
                    "temperature": 0.2,
                    "top_p": 0.9,
                }
            }

            # Call Ollama API
            try:
                async with httpx.AsyncClient(timeout=300.0) as client:
                    response = await client.post(OLLAMA_ENDPOINT, json=ollama_payload)
                    response.raise_for_status()

                    # Parse Ollama response
                    result = response.json()
                    generated_code = result.get("response", "")

                    # Clean up any markdown artifacts
                    generated_code = generated_code.strip()
                    if generated_code.startswith("```"):
                        lines = generated_code.split("\n")
                        if lines[0].startswith("```"):
                            lines = lines[1:]
                        if lines and lines[-1].strip() == "```":
                            lines = lines[:-1]
                        generated_code = "\n".join(lines)

                    return JSONResponse({
                        "jsonrpc": "2.0",
                        "id": msg_id,
                        "result": {
                            "content": [
                                {
                                    "type": "text",
                                    "text": generated_code
                                }
                            ]
                        }
                    })

            except httpx.HTTPError as e:
                return JSONResponse({
                    "jsonrpc": "2.0",
                    "id": msg_id,
                    "error": {
                        "code": -32603,
                        "message": f"Failed to connect to Ollama at {OLLAMA_ENDPOINT}: {e}"
                    }
                })
            except Exception as e:
                return JSONResponse({
                    "jsonrpc": "2.0",
                    "id": msg_id,
                    "error": {
                        "code": -32603,
                        "message": f"Unexpected error during code generation: {e}"
                    }
                })

        else:
            return JSONResponse({
                "jsonrpc": "2.0",
                "id": msg_id,
                "error": {
                    "code": -32601,
                    "message": f"Unknown method: {method}"
                }
            })

    except Exception as e:
        return JSONResponse({
            "jsonrpc": "2.0",
            "error": {
                "code": -32700,
                "message": f"Parse error: {e}"
            }
        })


# --- Starlette Application ---
app = Starlette(
    routes=[
        Route("/mcp", endpoint=handle_mcp_request, methods=["POST"]),
    ]
)


if __name__ == "__main__":
    print("=" * 80)
    print("MCP GPU Worker Dispatcher")
    print("=" * 80)
    print(f"Listening on: http://{LAPTOP_HOST}:{LAPTOP_PORT}")
    print(f"Forwarding to: {OLLAMA_ENDPOINT}")
    print(f"Model: deepseek-coder-v2")
    print("=" * 80)

    uvicorn.run(app, host=LAPTOP_HOST, port=LAPTOP_PORT, log_level="info")
