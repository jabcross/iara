#!/usr/bin/env python3
"""
MCP GPU Worker using FastMCP

This is the simplest working MCP server using the fastmcp library.
"""

import httpx
from fastmcp import FastMCP

# Configuration
DESKTOP_4090_IP = "10.68.110.22"
OLLAMA_ENDPOINT = f"http://{DESKTOP_4090_IP}:11434/api/generate"

# Create FastMCP server
mcp = FastMCP("GPU Worker")


@mcp.tool()
async def implement_code(instruction: str, code_context: str = "") -> str:
    """
    Generate code implementation based on instructions and context.

    Args:
        instruction: What code to implement
        code_context: Relevant code context
    """
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


if __name__ == "__main__":
    # Run with: python mcp_dispatcher_fastmcp.py
    # Serves HTTP (streamable) on localhost:9090 for SSH tunnel
    # The MCP endpoint will be at http://localhost:9090/mcp
    mcp.run(transport="http", host="localhost", port=9090)
