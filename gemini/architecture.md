I need you to act as a **Systems Architect** and **Network Engineer**.

**The Situation:**
I am developing on a remote Slurm-based HPC server using "VS Code Remote - SSH".
* **Constraint:** I cannot run heavy Python scripts or AI tools on the remote server due to memory limits.
* **Goal:** I want to run the **MCP Orchestrator** on my **Laptop**, but have the **Claude Code CLI** (running on the remote server) use it.

**My Topology:**
1.  **Remote Server (The Client):** Runs `claude` CLI. Needs to access the MCP tool.
2.  **Laptop (The Hub):** Runs the Python MCP Script. Connected to the Server via SSH.
3.  **Desktop 4090 (The Worker):** On the same LAN as the Laptop (`192.168.1.X`). Runs Ollama.

**Please generate a detailed setup guide containing:**

### Part 1: The "Reverse Tunnel" Configuration
* Provide the exact `ssh` flag or `.ssh/config` entry I need to add to my **Laptop** to enable **Remote Port Forwarding**.
* *Config Goal:* Map port `9090` on the Remote Server back to port `9090` on my Laptop.
* *Example:* `RemoteForward 9090 localhost:9090`.

### Part 2: The Laptop-Side Bridge Script (`laptop_dispatcher.py`)
* Write a Python script using `fastmcp` (or standard `mcp`) to run on the **Laptop**.
* **Tools to Expose:**
    * `implement_code(instruction, code_context)`: Receives request from Server -> Sends HTTP POST to Desktop 4090 (`http://<DESKTOP_IP>:11434/api/generate`) -> Returns code to Server.
* *Crucial:* The script must listen on `localhost` (port 9090) so the SSH tunnel can hit it.

### Part 3: The Remote Server Configuration
* Command to register the MCP tool in Claude Code on the **Remote Server**.
* *Key Detail:* The command must point to the *tunnel endpoint* on the server itself.
* *Command:* `claude mcp add gpu-worker --transport http http://localhost:9090/sse` (or appropriate endpoint).

### Part 4: The CLAUDE.md (Remote Side)
* Generate the `CLAUDE.md` file for my remote project root.
* **Rule:** "You are the Architect. Do NOT write code. Delegate all implementation to the `implement_code` tool. Note that this tool is running remotely, so be concise with context."

### Part 5: Connectivity Check
* A `curl` command to run on the **Remote Server** that attempts to hit `localhost:9090`. If this works, it proves the tunnel to the laptop is active.