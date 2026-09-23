@echo off
setlocal
title Ralph Loop Launcher

rem ============================================================
rem  Ralph Loop Launcher
rem  Drop this file in any project folder and double-click it.
rem  1. Creates PROMPT.md if it doesn't exist
rem  2. Pre-accepts the "trust this folder" dialog for this folder
rem  3. Adds the loop state file to .gitignore
rem  4. Makes a git checkpoint commit
rem  5. Starts Claude Code with permissions skipped and kicks off the loop
rem ============================================================

rem ---- Settings you can change ----
set "MAX_ITER=100"
set "PROMISE=COMPLETE"

rem ---- Always run from the folder this .bat lives in ----
cd /d "%~dp0"
set "RALPH_PROJ=%CD%"

echo ============================================================
echo  Ralph Loop Launcher
echo  Project: %CD%
echo  Max iterations: %MAX_ITER%
echo ============================================================
echo.

rem ---- Make sure Claude Code is installed ----
where claude >nul 2>&1
if errorlevel 1 goto :noclaude

rem ---- Create PROMPT.md only if it doesn't already exist ----
if exist "PROMPT.md" goto :promptexists
call :writeprompt
echo [OK]   Created PROMPT.md
goto :trust

:promptexists
echo [OK]   PROMPT.md already exists, leaving it as is

rem ---- Pre-accept workspace trust in %USERPROFILE%\.claude.json ----
rem      (runs the PowerShell section at the bottom of this file)
:trust
set "PSEXE=powershell"
where pwsh >nul 2>&1
if not errorlevel 1 set "PSEXE=pwsh"
%PSEXE% -NoProfile -ExecutionPolicy Bypass -Command "$s=[IO.File]::ReadAllText('%~f0'); $m='#'+'PS1START#'; iex $s.Substring($s.LastIndexOf($m)+$m.Length)"

:gitsetup
where git >nul 2>&1
if errorlevel 1 goto :nogit

if not exist ".git" git init >nul 2>&1
if not exist ".git" goto :nogit

findstr /x /c:".claude/ralph-loop.local.md" ".gitignore" >nul 2>&1
if errorlevel 1 (
    >>".gitignore" echo.
    >>".gitignore" echo .claude/ralph-loop.local.md
    echo [OK]   Added loop state file to .gitignore
)

git add -A >nul 2>&1
git commit -m "Checkpoint before Ralph loop" --allow-empty >nul 2>&1
if errorlevel 1 goto :commitfail
echo [OK]   Git checkpoint created
goto :donecheck

:commitfail
echo [WARN] Git commit failed. Run these once, then rerun this file:
echo        git config --global user.name "Your Name"
echo        git config --global user.email "you@example.com"
goto :donecheck

:nogit
echo [WARN] git not available, skipping checkpoint. No rollback point!

:donecheck
if exist ".ralph-done" echo [WARN] .ralph-done exists from a previous run. Delete it if the project is not actually finished.

echo.
echo Starting Claude Code...
echo To stop the loop early, type:  /ralph-loop:cancel-ralph
echo.

call claude --dangerously-skip-permissions "/ralph-loop:ralph-loop 'Read PROMPT.md and follow it exactly.' --completion-promise %PROMISE% --max-iterations %MAX_ITER%"

endlocal
exit /b 0

:noclaude
echo [ERROR] 'claude' was not found on PATH. Install Claude Code first.
pause
exit /b 1

rem ============================================================
rem  PROMPT.md contents
rem ============================================================
:writeprompt
> "PROMPT.md" echo # Autonomous build instructions
>>"PROMPT.md" echo.
>>"PROMPT.md" echo You are working unattended. The user is NOT available. Never ask questions or wait for confirmation.
>>"PROMPT.md" echo.
>>"PROMPT.md" echo ## Rules
>>"PROMPT.md" echo 1. Read all spec/design .md files in this repo to understand the goal.
>>"PROMPT.md" echo 2. Read TODO.md and DECISIONS.md if they exist, to see where the previous iteration left off.
>>"PROMPT.md" echo 3. When anything is ambiguous, make the best engineering/design choice yourself and log it in DECISIONS.md with a one-line reason. Do not stop to ask.
>>"PROMPT.md" echo 4. Keep TODO.md up to date: remaining tasks, in-progress, done.
>>"PROMPT.md" echo 5. Work in small steps. After each meaningful step: build, run tests, fix failures, then git commit with a clear message.
>>"PROMPT.md" echo 6. If something is blocked (missing dependency, broken tool), work around it or move to another task and note it in TODO.md.
>>"PROMPT.md" echo.
>>"PROMPT.md" echo ## Definition of done
>>"PROMPT.md" echo - Every item in the spec is implemented
>>"PROMPT.md" echo - The project builds with zero errors
>>"PROMPT.md" echo - All tests pass
>>"PROMPT.md" echo - TODO.md has no remaining items
>>"PROMPT.md" echo.
>>"PROMPT.md" echo ## Completion signal
>>"PROMPT.md" echo ONLY when every item above is genuinely true:
>>"PROMPT.md" echo 1. Create an empty file named .ralph-done in the repo root
>>"PROMPT.md" echo 2. Output exactly: ^<promise^>COMPLETE^</promise^>
>>"PROMPT.md" echo.
>>"PROMPT.md" echo Never output the promise to escape the loop early. If not done, just keep working.
exit /b 0

rem ============================================================
rem  PowerShell section: never run by cmd (every path above exits first).
rem  Read and executed by the :trust step. Marks this folder as trusted
rem  in %USERPROFILE%\.claude.json (case-sensitive parse). Backs it up first.
rem ============================================================
#PS1START#
try {
    $cfg  = Join-Path $env:USERPROFILE '.claude.json'
    $proj = $env:RALPH_PROJ.TrimEnd('\')
    $fwd  = $proj -replace '\\', '/'
    $keys = [System.Collections.Generic.List[string]]::new()
    foreach ($k in @($fwd, ($fwd.Substring(0,1).ToLower() + $fwd.Substring(1)), $proj)) {
        if (-not $keys.Contains($k)) { $keys.Add($k) }
    }

    if (-not (Test-Path $cfg)) {
        Write-Host '[WARN] ~/.claude.json not found (Claude never run on this PC?). Trust prompt may appear once.'
        return
    }

    $raw = [IO.File]::ReadAllText($cfg)

    # Case-sensitive JSON parse (the config has keys that differ only by case)
    if ($PSVersionTable.PSVersion.Major -ge 7) {
        $json = $raw | ConvertFrom-Json -AsHashtable -Depth 100
        $toText   = { param($o) $o | ConvertTo-Json -Depth 100 }
        $fromText = { param($t) $null = $t | ConvertFrom-Json -AsHashtable -Depth 100 }
    } else {
        Add-Type -AssemblyName System.Web.Extensions
        $js = New-Object System.Web.Script.Serialization.JavaScriptSerializer
        $js.MaxJsonLength  = [int]::MaxValue
        $js.RecursionLimit = 1000
        $json = $js.DeserializeObject($raw)
        $toText   = { param($o) $js.Serialize($o) }
        $fromText = { param($t) $null = $js.DeserializeObject($t) }
    }

    if (-not ($json.Keys -ccontains 'projects')) {
        $json['projects'] = New-Object 'System.Collections.Generic.Dictionary[string,object]'
    }
    $projects = $json['projects']

    $changed = $false
    foreach ($k in $keys) {
        if ($projects.Keys -ccontains $k) {
            $entry = $projects[$k]
            if ($entry['hasTrustDialogAccepted'] -ne $true) {
                $entry['hasTrustDialogAccepted'] = $true
                $changed = $true
            }
        } else {
            $entry = New-Object 'System.Collections.Generic.Dictionary[string,object]'
            $entry['hasTrustDialogAccepted'] = $true
            $projects[$k] = $entry
            $changed = $true
        }
    }

    if (-not $changed) {
        Write-Host '[OK]   Folder already trusted'
        return
    }

    $out = & $toText $json
    & $fromText $out
    Copy-Item $cfg "$cfg.ralph-backup" -Force
    [IO.File]::WriteAllText($cfg, $out, (New-Object System.Text.UTF8Encoding $false))
    Write-Host '[OK]   Folder marked as trusted (backup: .claude.json.ralph-backup)'
}
catch {
    Write-Host "[WARN] Could not pre-trust folder: $($_.Exception.Message)"
    Write-Host '       Not fatal. If the trust prompt appears, just press Enter.'
}