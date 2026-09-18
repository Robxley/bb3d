#!/usr/bin/env python3
"""
Orchestrator script for delegating tasks to Mistral Vibe CLI.
Supports both 'fixer' (TDD implementer) and 'reviewer' (code reviewer) roles,
with isolated git worktree support, glm-5.2 model default, and automatic log capture.
"""

import argparse
import datetime
import json
import os
import subprocess
import sys
from pathlib import Path

# Ensure UTF-8 output encoding across Windows consoles and pipes
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")


def find_vibe_executable() -> str:
    """Find the path to vibe.exe in PATH or common local locations."""
    # Check PATH
    import shutil
    vibe_path = shutil.which("vibe")
    if vibe_path:
        return vibe_path

    # Check Windows user local bin
    user_local = Path(os.environ.get("USERPROFILE", "")) / ".local" / "bin" / "vibe.exe"
    if user_local.exists():
        return str(user_local)

    raise FileNotFoundError("Could not find 'vibe' CLI executable. Ensure it is installed and in PATH.")


def read_task_info(task_file: Path) -> dict:
    """Extract metadata and context from the task markdown file."""
    content = task_file.read_text(encoding="utf-8")
    lines = content.splitlines()

    title = task_file.stem
    for line in lines:
        if line.startswith("# "):
            title = line[2:].strip()
            break

    is_bug_fix = any(kw in title.lower() or kw in content.lower() for kw in ["bug", "fix", "correctif", "anomalie"])
    
    return {
        "title": title,
        "is_bug_fix": is_bug_fix,
        "content": content,
    }


def generate_prompt(task_file: Path, role: str, task_info: dict, extra_prompt: str = "") -> str:
    """Generate an unambiguous, guideline-enforcing prompt for Vibe."""
    task_rel_path = os.path.relpath(task_file, os.getcwd()).replace("\\", "/")

    if role == "fixer":
        double_check_instructions = ""
        if task_info["is_bug_fix"]:
            double_check_instructions = f"""
CRITICAL - DOUBLE CHECK PROTOCOL (Zero Blind Trust):
This task is a bug fix. You MUST critically verify whether the reported bug actually exists in the current code BEFORE making any code edits.
1. Inspect the live code and reproduce or confirm the issue.
2. Fill in section '1.bis Revalidation Critique du Bug' in '{task_rel_path}' with your independent findings.
3. Check '[x] CONFIRMÉ' if the bug is real, or '[x] RÉFUTÉ' with technical justification if it is a false positive.
4. If refuted, do NOT modify engine code; document your reasoning in the task file.
"""

        prompt = f"""Assignment for bb3d-fixer:
Task File: {task_rel_path}
Task Title: {task_info['title']}

{double_check_instructions}

Implementation Instructions (TDD Workflow):
1. Review '{task_rel_path}' and strictly obey all guidelines in 'AGENTS.md'.
   Focus EXCLUSIVELY on implementing the code changes for '{task_rel_path}'. Do NOT create or modify other task files.
2. If section 1.bis is already confirmed, proceed directly to code implementation and test verification.
3. Write/update the unit test in 'tests/unit_test_*.cpp'.
3. Build and test to verify failure:
   cmake --build build --config Debug -j && ctest --test-dir build -C Debug --output-on-failure
4. Implement the minimal necessary C++20 code in 'src/bb3d/' and 'include/bb3d/' (GREEN).
   - Public API opacity (no Vulkan/SDL in client headers)
   - Zero allocation in hot paths
   - Vulkan Sync2 (pipelineBarrier2 + DependencyInfo only)
   - Multi-streams vertex layout (VertexPos 12 bytes)
5. Re-run CTest to verify all tests pass without regressions.
6. Update the task file '{task_rel_path}':
   - Check all applicable boxes under 'Checkpoints de l'Implémenteur'.
   - Set task status to '[READY FOR CODE REVIEW]'.
   - Add an entry in 'Journal des Échanges'.
7. Commit your changes atomically with a descriptive conventional commit message.

{extra_prompt}
"""
    elif role == "reviewer":
        prompt = f"""Assignment for bb3d-reviewer:
Task File: {task_rel_path}
Task Title: {task_info['title']}

Review Instructions (Strict Read-Only Inspection & Reporting Mode):
1. PURE REVIEW FOR CODE 0:
   - Your sole responsibility is to inspect the code changes and produce a structured review report in your assistant response.
   - Do NOT run 'cmake --build' or any recompilation commands (the build and test verification has already been conducted).
   - Do NOT modify ANY files: do NOT edit C++, headers, markdown docs, or task files.
2. Inspect the changes using:
   - 'git diff main...HEAD' (or 'git diff HEAD~1')
   - Read the task specification in '{task_rel_path}'
   - Read relevant source or test files if needed.
3. Verify:
   - Bug Double Check: Is section '1.bis' filled with proof if this is a fix?
   - Architecture & Opacity: Are Vulkan/SDL types hidden from client headers?
   - C++20 Standards: Zero hot-path allocations, std::span / std::string_view, designated initializers?
   - Vulkan Modern Standards: Synchronization2 (pipelineBarrier2), no descriptor leaks, no blocking waitIdle()?
   - Code hygiene: technical English comments/logs, no orphaned includes or dead code.
4. Deliverable - Structured Text Report in Assistant Response:
   - Output a clear, comprehensive markdown report directly in your response with:
     * Executive Summary
     * Checkpoints Verification (Double-Check, Architecture, C++20, Vulkan Modern, Hygiene)
     * Strengths & Observations / Risks (if any)
     * Verdict: [APPROVED] or [CHANGES REQUESTED] (with actionable list for fixer)
   - Do NOT edit the task file on disk; the orchestrator will double-check your report and apply all updates.
   - Conclude your report and stop to ensure a clean exit code 0.

{extra_prompt}
"""
    else:
        raise ValueError(f"Unknown role: {role}. Expected 'fixer' or 'reviewer'.")

    return prompt.strip()


def run_vibe(
    task_file: Path,
    role: str = "fixer",
    model: str = "glm-5.2",
    agent: str = None,
    worktree: str = None,
    max_turns: int = 40,
    extra_prompt: str = "",
    timeout_sec: int = 900,
) -> int:
    """Execute Mistral Vibe CLI with the specified configuration."""
    vibe_bin = find_vibe_executable()

    if not task_file.exists():
        print(f"[-] Error: Task file '{task_file}' does not exist.", file=sys.stderr)
        return 1

    task_info = read_task_info(task_file)
    prompt = generate_prompt(task_file, role, task_info, extra_prompt)

    # Determine default agent persona if not specified
    if not agent:
        agent = "bb3d-fixer" if role == "fixer" else "bb3d-reviewer"

    # Setup logs directory
    logs_dir = Path("tasks/active/logs")
    logs_dir.mkdir(parents=True, exist_ok=True)
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    log_file = logs_dir / f"{task_file.stem}_{role}_{timestamp}.log"
    raw_log_file = logs_dir / f"{task_file.stem}_{role}_{timestamp}.raw.jsonl"
    status_file = logs_dir / "live_status.json"

    # Build CLI command with streaming output for real-time monitoring
    cmd = [
        vibe_bin,
        "-p", prompt,
        "--agent", agent,
        "--auto-approve",
        "--trust",
        "--output", "streaming",
        "--max-turns", str(max_turns),
    ]

    if worktree:
        cmd.extend(["--worktree", worktree])

    env = os.environ.copy()
    env["VIBE_ACTIVE_MODEL"] = model
    env["PYTHONUNBUFFERED"] = "1"
    env["PYTHONIOENCODING"] = "utf-8"

    print(f"[+] Launching Vibe CLI ({role.upper()}) on task: {task_file.name}")
    print(f"    - Model: {model}")
    print(f"    - Agent: {agent}")
    if worktree:
        print(f"    - Worktree: {worktree}")
    print(f"    - Log file: {log_file}")
    print(f"    - Live status: {status_file}")
    print("-" * 60)

    def write_status(state: str, action: str):
        try:
            status_data = {
                "task": task_file.name,
                "role": role,
                "agent": agent,
                "state": state,
                "last_action": action,
                "updated_at": datetime.datetime.now().isoformat(),
                "log_file": str(log_file),
            }
            status_file.write_text(json.dumps(status_data, indent=2), encoding="utf-8")
        except Exception:
            pass

    write_status("RUNNING", "Started agent process")

    def format_event(event: dict) -> tuple[str, str]:
        """Convert a Vibe streaming JSON event into a human-readable string and brief action summary."""
        try:
            etype = event.get("type")
            now = datetime.datetime.now().strftime("%H:%M:%S")

            if etype == "reasoning":
                text = (event.get("text") or "").strip()
                first_line = text.splitlines()[0] if text else ""
                return f"[{now}] 💭 [Thinking] {text}\n", f"Thinking: {first_line[:60]}"

            elif etype == "effect":
                detail_dict = event.get("detail") or {}
                tool_name = event.get("title") or detail_dict.get("toolName", "tool")
                inputs = detail_dict.get("input") or {}
                state = event.get("state") or {}
                status = state.get("status", "running")
                dur = state.get("durationMs")
                dur_str = f" ({dur:.0f}ms)" if dur else ""

                if isinstance(inputs, dict):
                    if "command" in inputs:
                        detail = str(inputs["command"])
                    elif "filePath" in inputs:
                        detail = str(inputs["filePath"])
                    elif "path" in inputs:
                        detail = str(inputs["path"])
                    else:
                        detail = json.dumps(inputs)
                else:
                    detail = str(inputs)

                action_summary = f"{tool_name}: {detail[:60]}"
                msg = f"[{now}] 🔧 [{tool_name}] {detail} -> status={status}{dur_str}\n"

                out = state.get("outputText")
                if not out:
                    output_dict = state.get("output")
                    if isinstance(output_dict, dict):
                        out = output_dict.get("stdout") or output_dict.get("output") or ""
                    elif isinstance(output_dict, str):
                        out = output_dict

                if out and str(out).strip():
                    lines = str(out).strip().splitlines()
                    preview = "\n      ".join(lines[:6])
                    if len(lines) > 6:
                        preview += f"\n      ... (+{len(lines) - 6} more lines)"
                    msg += f"   Output:\n      {preview}\n"

                return msg, action_summary

            elif etype == "message" and event.get("role") == "assistant":
                content_list = event.get("content") or []
                parts = []
                for c in content_list:
                    if isinstance(c, dict) and c.get("type") == "text":
                        parts.append(c.get("text", ""))
                    elif isinstance(c, str):
                        parts.append(c)
                msg = "".join(parts).strip()
                return f"[{now}] 🤖 [Assistant] {msg}\n", f"Assistant message ({len(msg)} chars)"

        except Exception as e:
            return f"[{now}] [EVENT {event.get('type')}] {e}\n", f"Event {event.get('type')}"

        return "", ""


    try:
        with open(log_file, "w", encoding="utf-8") as lf, open(raw_log_file, "w", encoding="utf-8") as rf:
            process = subprocess.Popen(
                cmd,
                stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                encoding="utf-8",
                errors="replace",
                env=env,
            )

            for raw_line in process.stdout:
                rf.write(raw_line)
                rf.flush()

                stripped = raw_line.strip()
                if not stripped:
                    continue

                # Try parsing as streaming JSON
                if stripped.startswith("{") and stripped.endswith("}"):
                    try:
                        event = json.loads(stripped)
                        formatted, action_summary = format_event(event)
                        if formatted:
                            sys.stdout.write(formatted)
                            sys.stdout.flush()
                            lf.write(formatted)
                            lf.flush()
                        if action_summary:
                            write_status("RUNNING", action_summary)
                        continue
                    except json.JSONDecodeError:
                        pass

                # Fallback to plain text line
                sys.stdout.write(raw_line)
                sys.stdout.flush()
                lf.write(raw_line)
                lf.flush()

            process.wait(timeout=timeout_sec)
            ret_code = process.returncode

        print("-" * 60)
        if ret_code == 0:
            print(f"[+] Vibe CLI completed successfully (exit code 0).")
            write_status("COMPLETED", "Task completed successfully")
        else:
            print(f"[-] Vibe CLI exited with code {ret_code}. See log: {log_file}", file=sys.stderr)
            write_status("FAILED", f"Exited with code {ret_code}")
        return ret_code

    except subprocess.TimeoutExpired:
        print(f"[-] Error: Vibe CLI timed out after {timeout_sec} seconds.", file=sys.stderr)
        write_status("TIMEOUT", f"Timed out after {timeout_sec}s")
        process.kill()
        return 124
    except Exception as e:
        print(f"[-] Error executing Vibe CLI: {e}", file=sys.stderr)
        write_status("ERROR", str(e))
        return 1


def main():
    parser = argparse.ArgumentParser(description="Orchestrate Mistral Vibe agents for bb3d project tasks.")
    parser.add_argument("--task", required=True, type=Path, help="Path to the task file in tasks/active/TASK-*.md")
    parser.add_argument("--role", choices=["fixer", "reviewer"], default="fixer", help="Agent role: fixer or reviewer")
    parser.add_argument("--model", default="glm-5.2", help="Model to use (default: glm-5.2)")
    parser.add_argument("--agent", default=None, help="Custom agent name (default: bb3d-fixer or bb3d-reviewer)")
    parser.add_argument("--worktree", default=None, help="Optional git worktree name for isolated branch execution")
    parser.add_argument("--max-turns", type=int, default=40, help="Max assistant turns (default: 40)")
    parser.add_argument("--timeout", type=int, default=900, help="Timeout in seconds (default: 900)")
    parser.add_argument("--extra-prompt", default="", help="Additional prompt instructions or feedback")

    args = parser.parse_args()
    ret = run_vibe(
        task_file=args.task,
        role=args.role,
        model=args.model,
        agent=args.agent,
        worktree=args.worktree,
        max_turns=args.max_turns,
        extra_prompt=args.extra_prompt,
        timeout_sec=args.timeout,
    )
    sys.exit(ret)


if __name__ == "__main__":
    main()
