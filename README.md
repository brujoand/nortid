nortid
======

A Pebble watchface displaying time and date in Norwegian text.

| Aplite | Basalt | Diorite | Chalk | Emery |
|:------:|:------:|:-------:|:-----:|:-----:|
| ![aplite](https://github.com/brujoand/nortid/releases/latest/download/aplite.png) | ![basalt](https://github.com/brujoand/nortid/releases/latest/download/basalt.png) | ![diorite](https://github.com/brujoand/nortid/releases/latest/download/diorite.png) | ![chalk](https://github.com/brujoand/nortid/releases/latest/download/chalk.png) | ![emery](https://github.com/brujoand/nortid/releases/latest/download/emery.png) |

## Install

Download `nortid.pbw` from the [latest release](https://github.com/brujoand/nortid/releases/latest) and sideload via the Pebble/Rebble app.

## Build

```bash
pip install pebble-tool
pebble sdk install latest
pebble build
```

## Test

```bash
make test
```

## Commit messages

Commits must follow [Conventional Commits](https://www.conventionalcommits.org/) to drive automatic versioning:

- `fix: ...` — patch bump (e.g. 1.0.0 → 1.0.1)
- `feat: ...` — minor bump (e.g. 1.0.0 → 1.1.0)
- `feat!: ...` or `BREAKING CHANGE` in body — major bump (e.g. 1.0.0 → 2.0.0)
- Other prefixes: `docs`, `style`, `refactor`, `perf`, `test`, `build`, `ci`, `chore`, `revert`
