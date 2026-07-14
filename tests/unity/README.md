# Vendored: ThrowTheSwitch/Unity

Byte-exact copy of `src/unity.{c,h}`, `src/unity_internals.h`, and `LICENSE.txt`
from https://github.com/ThrowTheSwitch/Unity at commit
`d1fe18bd54434efd1ac0dad035d3ab0f8591e086` — the same commit the former git
submodule pinned. Vendored instead of a submodule so `make test` needs no
`submodule update --init` and `git worktree remove` works (git refuses to
remove worktrees containing submodules).

Do not edit these files. To upgrade, replace them with a newer upstream
commit's copies and update the commit hash here. They are excluded from
clang-format and the whitespace fixers in `.pre-commit-config.yaml` to stay
diffable against upstream.
