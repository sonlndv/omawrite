# Omawrite

A dead-simple Markdown writing app built with Qt Quick and C++ that automatically follows system dark/light mode.

<img width="2948" height="3227" alt="screenshot-2026-06-23_15-24-08" src="https://github.com/user-attachments/assets/4e930c0d-edda-4046-b444-a59eff523329" />
<img width="2948" height="3227" alt="screenshot-2026-06-23_15-23-23" src="https://github.com/user-attachments/assets/8ced7c26-961b-4ded-b263-84403001a951" />


## Install

Install via the Omarchy Package Repository via the `omawrite` package. It's installed by default in new installations of Omarchy (from Quattro forward).

## Shortcuts

- `Ctrl+S` saves. Unsaved documents use the XDG desktop portal file picker.
- `Ctrl+Shift+S` saves as.
- `Ctrl+O` opens a Markdown file through the portal picker.
- `Ctrl+P` opens the system print dialog.
- `Ctrl+N` opens a new Omawrite window.
- `Ctrl+Z`, `Ctrl+Shift+Z`, and `Ctrl+Y` handle undo and redo.
- `Super+F` toggles fullscreen. Qt maps this key as `Meta+F`.
- `Ctrl+F` searches the document. Use `Enter` or `Ctrl+G` for the next match and `Shift+Enter` for the previous match.
- `Ctrl+H` opens find and replace.
- `Ctrl+B`, `Ctrl+I`, and `Ctrl+K` insert bold, italic, and link Markdown.
- `Ctrl+?` shows the keyboard shortcut reference.

## Wikilinks

Typing `[[Note Name]]` or `[[Note Name|alias]]` creates a link to another note
in the vault, styled with the theme's accent color. Ctrl+click a link, or
place the cursor inside one and press Enter, to open it; if no matching note
exists you're offered to create it, Obsidian-style.

A link resolves by matching the vault-wide filename stem (name without
`.md`), case-insensitive. If more than one note shares that stem, the
shallowest path (fewest folders deep) wins; remaining ties break
alphabetically by path for a deterministic result.

Unsaved drafts are recovered after an abnormal exit. Omawrite also watches open files
and warns before an external change can replace local work.

The font button in the footer picks the writing font from any installed text font,
and Omawrite remembers the choice. IBM Plex Mono is the default.

Text follows the desktop text size — `omarchy display text size`, or GNOME's
`text-scaling-factor` — and re-flows without a restart. The default of 12px leaves
Omawrite at the size it is designed around; larger and smaller sizes scale from there.

## Requirements

- Qt 6: `qt6-base`, `qt6-declarative`, `qt6-quickcontrols2`
- `xdg-desktop-portal` and a portal backend

The IBM Plex Mono font is bundled under the SIL Open Font License 1.1; see
`fonts/OFL.txt`. The font is copyright IBM Corp.

## Fork maintenance

This checkout is a fork of upstream `omacom-io/omawrite` carrying local
features (vault, wikilinks, link graph, quick switcher) not yet upstream.
The machine also has upstream `omawrite` available from the `omarchy`
pacman repo, and `omarchy-update` / `pacman -Syu` will try to keep that
package current. To stop an update from silently overwriting this fork's
binary at `/usr/bin/omawrite`, this fork is packaged under a **different
pacman package name**, `omawrite-son`, built from `pkgbuild/PKGBUILD`:

- `pkgname=omawrite-son` — a distinct package pacman tracks independently
  from upstream `omawrite`.
- `provides=('omawrite')` and `conflicts=('omawrite')` — pacman treats
  `omawrite-son` as satisfying anything that depends on `omawrite`, and
  will refuse to have both installed at once. Exactly one of
  upstream `omawrite` or this fork's `omawrite-son` owns `/usr/bin/omawrite`
  at a time; pacman enforces that, not a hand install.
- A plain `pacman -Syu` only touches packages pacman knows about by name.
  Because this fork is a different name, `omarchy-update` reinstalling or
  upgrading upstream `omawrite` is a normal, uneventful transaction for
  pacman — it does not touch `omawrite-son`, and it does not get to put
  its binary back at `/usr/bin/omawrite` while `omawrite-son` is installed
  and has not been removed (the conflict would require explicit
  `pacman -S omawrite`, which prompts to remove `omawrite-son` first).
- This is deliberately **reversible**: `pacman -R omawrite-son` fully
  uninstalls the fork and leaves the box exactly as if upstream `omawrite`
  were reinstalled fresh. An epoch-bump approach (same pkgname, higher
  epoch) was considered and rejected — it would re-fight the upstream
  repo's version on every sync instead of just existing as its own package.

### Rebase-on-upstream loop

1. Add/update the upstream remote and fetch:
   `git remote add upstream https://github.com/omacom-io/omawrite.git` (once),
   then `git fetch upstream`.
2. Rebase local feature work onto upstream's latest tag/branch:
   `git rebase upstream/main` (resolve conflicts commit by commit — local
   features touch `src/backend.*`, `src/linkindex.*`, QML files, and
   `pkgbuild/`; upstream changes to the same files are the usual conflict
   source).
3. Re-run the test suite (`./bin/test`) and the build (`./bin/build`)
   before repackaging.
4. Rebuild the package: `cd pkgbuild && makepkg -f`. `pkgver()` derives the
   version from `git describe --long --tags`, so it updates automatically
   once the rebase picks up new upstream tags.
5. Install the new build to replace the previous `omawrite-son`:
   `sudo pacman -U pkgbuild/omawrite-son-<version>-x86_64.pkg.tar.zst`
   (upgrading `omawrite-son` in place; no `-R`/`-S` dance needed since it's
   the same package name across rebuilds).
6. Verify: `pacman -Qo $(which omawrite)` should still report
   `omawrite-son`, and `which omawrite` should resolve to `/usr/bin/omawrite`
   with the rebuilt binary (check `omawrite --version`-equivalent behavior
   or just confirm the new features run).
