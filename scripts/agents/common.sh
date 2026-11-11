# Shared settings for the scripts in scripts/agents/. Source this file; do not run it.
#
#   LUTHIER_WT_ROOT   directory that holds worker worktrees and the machine lock
#                     (default: <repo parent>/luthier-worktrees)
#   LUTHIER_JOBS      parallel compile jobs for one build (default: CPU count)
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# A worktree's scripts must still find the main checkout that owns ThirdParty/.
MAIN_ROOT="$(cd "$REPO_ROOT" && dirname "$(git rev-parse --path-format=absolute --git-common-dir)")"
WT_ROOT="${LUTHIER_WT_ROOT:-$(dirname "$MAIN_ROOT")/luthier-worktrees}"
LOCK_FILE="$WT_ROOT/.machine.lock"
JOBS="${LUTHIER_JOBS:-$(nproc)}"
mkdir -p "$WT_ROOT"
