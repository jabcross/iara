I need you to act as a **Systems Architect** and **Python Developer**. Help me configure a "Remote Compute" workflow for **Claude Code**.

**The Objective:**
I want to decrease my usage of the Claude Sonnet 4.5 model by offloading code implementation tasks to my local RTX 4090. Sonnet should act *only* as the Architect/Planner.

**My Infrastructure (2-Node Setup):**
1.  **Head Node (Dev Server):**
    * **OS:** Linux (Ubuntu/Debian).
    * **Role:** Runs `claude` CLI and the MCP Bridge Script.
    * **Constraint:** I do NOT have root access. I cannot install system packages or edit `/etc` files. All tools must run in user space (virtualenv).
    * **Access:** I connect to this via VS Code Remote - SSH.
2.  **Worker Node (The GPU):**
    * **Hardware:** Desktop with **RTX 4090 (24GB)**.
    * **IP:** `192.168.1.X` (Placeholder).
    * **Role:** Runs Ollama with `Qwen 2.5 Coder 32B`.

**Please generate a Step-by-Step Implementation Guide containing:**

### Part 1: Worker Node Configuration (RTX 4090)
* Provide the command to configure **Ollama** to listen on `0.0.0.0` (Local Network) instead of localhost.
    * *Include specific instructions for Linux (systemd) vs Windows (Environment Variables).*
* Provide the command to pull the model: `qwen2.5-coder:32b`.

### Part 2: The MCP Bridge Script (`gpu_bridge.py`)
* **Environment Setup:** Provide the exact commands to create a local Python virtual environment (`venv`) on the Dev Server and install the necessary libraries (`mcp`, `httpx`) without sudo.
* **The Script:** Write a robust Python script to run on the **Head Node**.
* **Tools to Expose:**
    1.  `implement_code(instruction: str, file_context: str) -> str`:
        * Sends a POST request to the Worker Node's Ollama API (`http://<WORKER_IP>:11434/api/generate`).
        * System Prompt: "You are an expert coder. Return ONLY the code implementation based on the instructions. Do not include markdown conversational filler."
* *Requirement:* Use `httpx` with a generous timeout (e.g., 120 seconds) because 32B models can take time to generate large blocks.

### Part 3: Claude Code Integration (User Space)
* **Register:** The specific `claude mcp add` command that points to the python executable *inside* the virtual environment (e.g., `/home/user/my_project/.venv/bin/python`).
* **Govern:** Create a `CLAUDE.md` file for the project root.
    * **Strict Rule:** "You are the Architect. You perform reasoning and planning. When you are ready to write code, you MUST use the `implement_code` tool. Do not generate code blocks directly in the chat."

### Part 4: Verification
* A `curl` command to run from the **Head Node** that sends a simple "Hello" to the **Worker Node** to prove the network is open before I spend time debugging the Python script.
