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
2. Write a failing unit test in 'tests/unit_test_*.cpp' demonstrating the bug or new feature (RED).
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

Review Instructions (Systematic Code Review):
1. You are in read-only review mode. Do NOT modify any C++ or header files.
2. Inspect the latest git diff and unit tests created for this task.
3. Verify:
   - Bug Double Check: Did the implementer verify and document section '1.bis' if this is a fix?
   - Architecture & Opacity: Are Vulkan types hidden from client headers?
   - C++20 Standards: Zero hot-path allocations, std::span / std::string_view zero-copy, designated initializers?
   - Vulkan Modern Standards: Synchronization2 (pipelineBarrier2), no descriptor leaks, no blocking waitIdle()?
   - Tests: Run 'ctest --test-dir build -C Debug --output-on-failure' to confirm tests pass.
4. Deliverable:
   - Update '{task_rel_path}': Check off verified boxes under 'Checkpoints des Reviewers'.
   - Add a signed review entry in 'Journal des Échanges' with date and constructive feedback.
   - Render decision: '[APPROVED]' (and update task status to '[DONE]') or '[CHANGES REQUESTED]' with specific actionable items.

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
    max_turns: int = 25,
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

    # Build CLI command
    cmd = [
        vibe_bin,
        "-p", prompt,
        "--agent", agent,
        "--auto-approve",
        "--trust",
        "--max-turns", str(max_turns),
    ]

    if worktree:
        cmd.extend(["--worktree", worktree])

    env = os.environ.copy()
    env["VIBE_ACTIVE_MODEL"] = model

    print(f"[+] Launching Vibe CLI ({role.upper()}) on task: {task_file.name}")
    print(f"    - Model: {model}")
    print(f"    - Agent: {agent}")
    if worktree:
        print(f"    - Worktree: {worktree}")
    print(f"    - Log file: {log_file}")
    print("-" * 60)

    try:
        with open(log_file, "w", encoding="utf-8") as lf:
            process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                encoding="utf-8",
                errors="replace",
                env=env,
            )

            # Stream output live and tee into log file
            for line in process.stdout:
                sys.stdout.write(line)
                sys.stdout.flush()
                lf.write(line)

            process.wait(timeout=timeout_sec)
            ret_code = process.returncode

        print("-" * 60)
        if ret_code == 0:
            print(f"[+] Vibe CLI completed successfully (exit code 0).")
        else:
            print(f"[-] Vibe CLI exited with code {ret_code}. See log: {log_file}", file=sys.stderr)
        return ret_code

    except subprocess.TimeoutExpired:
        print(f"[-] Error: Vibe CLI timed out after {timeout_sec} seconds.", file=sys.stderr)
        process.kill()
        return 124
    except Exception as e:
        print(f"[-] Error executing Vibe CLI: {e}", file=sys.stderr)
        return 1


def main():
    parser = argparse.ArgumentParser(description="Orchestrate Mistral Vibe agents for bb3d project tasks.")
    parser.add_argument("--task", required=True, type=Path, help="Path to the task file in tasks/active/TASK-*.md")
    parser.add_argument("--role", choices=["fixer", "reviewer"], default="fixer", help="Agent role: fixer or reviewer")
    parser.add_argument("--model", default="glm-5.2", help="Model to use (default: glm-5.2)")
    parser.add_argument("--agent", default=None, help="Custom agent name (default: bb3d-fixer or bb3d-reviewer)")
    parser.add_argument("--worktree", default=None, help="Optional git worktree name for isolated branch execution")
    parser.add_argument("--max-turns", type=int, default=25, help="Max assistant turns (default: 25)")
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
