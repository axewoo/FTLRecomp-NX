# axewoo's Switch ports for tico

A [tico](https://ticoverse.com) Store repository listing my Nintendo Switch homebrew ports, downloaded straight from
their GitHub releases:

| Package | From |
|---|---|
| FTL: Faster Than Light (bring your own `ftl.dat`) | [FTLRecomp-NX](https://github.com/axewoo/FTLRecomp-NX) |
| Shattered Pixel Dungeon | [ShatteredPD-NX](https://github.com/axewoo/ShatteredPD-NX) |
| Mindustry (proof of concept) | [Mindustry-NX](https://github.com/axewoo/Mindustry-NX) |
| Tarkov Hideout and Tarkov Hideout Lite | [TarkovHideout-NX](https://github.com/axewoo/TarkovHideout-NX) |

No game data is listed here: FTL needs `ftl.dat` from your own copy of the game.

## Add it to tico

In tico, open **Store › Sources › Add repository** and type:

```
raw.githubusercontent.com/axewoo/tico-repo/main/index.json
```

(tico adds `https://` itself.)

## How it stays up to date

`packages.json` says which release asset each package is. `tools/build_index.py` reads the newest release of each
port (version, download URL, size, SHA-256, release notes) and writes `index.json` in tico's
[repository format](https://ticoverse.com/wiki/store/creating-a-store-repository). The
`Update index.json` workflow runs it every 3 hours, on demand (Actions › Update index.json › Run workflow) and when
`packages.json` changes, and commits `index.json` when something changed. A new release of a port reaches tico on its
next refresh after that.

To add a port, add an entry to `packages.json` (`id` must stay the same forever) and push.
