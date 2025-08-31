#!/usr/bin/env bash
# commit_initial_strict.sh

set -euo pipefail

expected_name="admin"
expected_email="admin@pgelephant.com"
commit_msg='Issue (#1): Initial Commit.'

# inside a git repo?
git rev-parse --is-inside-work-tree >/dev/null 2>&1 || {
  echo "Not a git repo."
  exit 1
}

# local identity must match
name="$(git config --local user.name || true)"
email="$(git config --local user.email || true)"

if [[ "$name" != "$expected_name" || "$email" != "$expected_email" ]]; then
  echo "Abort: user.name or user.email mismatch."
  echo "Have: name='$name', email='$email'"
  exit 1
fi

# refuse if nothing staged
if git diff --cached --quiet; then
  echo "Abort: nothing staged to commit."
  exit 1
fi

# commit with fixed message, no empty commits allowed
git commit -m "$commit_msg"

