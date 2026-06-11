# Editor Support

This folder contains lightweight syntax highlighting and indentation support for
IAC/SPARTA-style input files.

The language is intentionally treated as a mixed input language: native SPARTA
commands such as `run`, `variable`, `stats_style`, and `surf_collide` appear
beside IAC commands such as `voxel_create`, `isthmus_surface`, `surf_flux`, and
`iac_run`.

## Doom Emacs

The Emacs mode lives in:

```text
editors/emacs/iac-input-mode.el
```

To load it from a Doom configuration, add this to `~/.doom.d/config.el`.
If your Doom setup is tangled from Org, put the block in `~/.doom.d/config.org`
instead so it survives the next tangle:

```elisp
;; IAC/SPARTA input files
(let ((iac-mode-file "/path/to/isthmus-ablation-core/editors/emacs/iac-input-mode.el"))
  (when (file-readable-p iac-mode-file)
    (load-file iac-mode-file)))
```

For this local checkout, Codex can add the same block with the absolute path to
this repository. After restarting Emacs, files named `in.*` and files ending in
`.iac` or `.sparta` open in `iac-input-mode`.

## VSCode

The VSCode language extension lives in:

```text
editors/vscode/iac-input
```

For local development, link it into VSCode's extension folder and restart VSCode:

```bash
mkdir -p ~/.vscode/extensions
ln -s /path/to/isthmus-ablation-core/editors/vscode/iac-input ~/.vscode/extensions/iac-input
```

The extension recognizes files named `in.*` and files ending in `.iac` or
`.sparta`.

If you prefer a packaged extension, install `vsce` and run this from the
extension folder:

```bash
vsce package
code --install-extension iac-input-0.1.0.vsix
```
