;;; iac-input-mode.el --- Major mode for IAC/SPARTA input files -*- lexical-binding: t; -*-

;; This file is intentionally dependency-free so Doom Emacs can load it directly
;; from a checkout of isthmus-ablation-core.

;;; Code:

(require 'subr-x)

(unless (boundp 'font-lock-number-face)
  (defvar font-lock-number-face 'font-lock-constant-face
    "Fallback face for numbers on Emacs builds without `font-lock-number-face'."))

(defvar iac-input-mode-syntax-table
  (let ((table (make-syntax-table)))
    (modify-syntax-entry ?# "<" table)
    (modify-syntax-entry ?\n ">" table)
    (modify-syntax-entry ?_ "_" table)
    (modify-syntax-entry ?- "_" table)
    table)
  "Syntax table for `iac-input-mode'.")

(defgroup iac-input nil
  "Editing support for IAC/SPARTA input files."
  :group 'languages)

(defcustom iac-input-indent-offset 2
  "Indentation offset for IAC/SPARTA input files."
  :type 'integer
  :safe #'integerp
  :group 'iac-input)

(defconst iac-input-iac-commands
  '("grid_dump"
    "grid_write_vtu"
    "iac_continue"
    "iac_limit"
    "iac_run"
    "iac_set"
    "iac_spa_stats"
    "iac_stats"
    "iac_stats_style"
    "iac_timestep"
    "iac_verify"
    "isthmus_surface"
    "source"
    "surf_dump"
    "surf_flux"
    "surf_install"
    "surf_measure_flux"
    "voxel_ablate"
    "voxel_create"
    "voxel_dump"
    "voxel_ghost"
    "voxel_material"
    "voxel_write_history"
    "voxel_write_vtu"))

(defconst iac-input-sparta-commands
  '("adapt_grid"
    "balance_grid"
    "boundary"
    "clear"
    "compute"
    "create_box"
    "create_grid"
    "create_particles"
    "dimension"
    "dump"
    "dump_modify"
    "fix"
    "global"
    "if"
    "include"
    "jump"
    "label"
    "mixture"
    "next"
    "read_surf"
    "remove_surf"
    "read_restart"
    "run"
    "seed"
    "shell"
    "species"
    "stats"
    "stats_modify"
    "stats_style"
    "surf_collide"
    "surf_modify"
    "surf_react"
    "timestep"
    "uncompute"
    "unfix"
    "variable"
    "write_restart"
    "write_surf"))

(defconst iac-input-keywords
  '("absolute"
    "active"
    "all"
    "axis"
    "boundary"
    "buffer"
    "carryover/normal"
    "constant"
    "crop"
    "delete"
    "density"
    "diagnostic"
    "diameter"
    "duration"
    "equal"
    "exact"
    "face"
    "false"
    "file"
    "fixed"
    "formula"
    "ghosted"
    "ghosts"
    "history"
    "infinite"
    "internal"
    "local"
    "map"
    "mass/courant"
    "material"
    "molar-mass"
    "norm"
    "percent"
    "policy"
    "real"
    "resolution"
    "scalar"
    "select"
    "slab"
    "source"
    "sphere"
    "surface"
    "then"
    "tiff"
    "time"
    "tolerance"
    "true"
    "units"
    "variable"
    "voxels"
    "vtu"
    "vtp"
    "weighting"))

(defvar iac-input-font-lock-keywords
  `(("^[[:space:]]*#.*$" . font-lock-comment-face)
    ("\\(&\\)\\s-*\\(?:#.*\\)?$" 1 font-lock-preprocessor-face)
    (,(concat "^\\s-*\\(" (regexp-opt iac-input-iac-commands t) "\\)\\_>")
     1 font-lock-keyword-face)
    (,(concat "^\\s-*\\(" (regexp-opt iac-input-sparta-commands t) "\\)\\_>")
     1 font-lock-builtin-face)
    (,(concat "\\_<\\(" (regexp-opt iac-input-keywords t) "\\)\\_>")
     1 font-lock-constant-face)
    ("\\${[^}[:space:]]+}" . font-lock-variable-name-face)
    ("\\_<[A-Za-z][A-Za-z0-9-]*:" . font-lock-label-face)
    ("\\_<[+-]?\\(?:[0-9]+\\.?[0-9]*\\|\\.[0-9]+\\)\\(?:[eE][+-]?[0-9]+\\)?\\_>"
     . font-lock-number-face)))

(defun iac-input--current-line-text ()
  "Return text from indentation to end of current line."
  (string-trim-left
   (buffer-substring-no-properties (line-beginning-position) (line-end-position))))

(defun iac-input--continuation-line-p (line)
  "Return non-nil if LINE continues the previous logical input command."
  (or (string-prefix-p "&" line)
      (and (not (string-empty-p line))
           (not (string-prefix-p "#" line))
           (save-excursion
             (forward-line -1)
             (string-suffix-p "&"
                              (string-trim-right
                               (buffer-substring-no-properties
                                (line-beginning-position)
                                (line-end-position))))))))

(defun iac-input--previous-code-line ()
  "Move to the previous nonblank, noncomment line and return its trimmed text."
  (let ((line nil))
    (save-excursion
      (while (and (not line) (> (line-number-at-pos) 1))
        (forward-line -1)
        (let ((text (string-trim-left
                     (buffer-substring-no-properties
                      (line-beginning-position)
                      (line-end-position)))))
          (unless (or (string-empty-p text) (string-prefix-p "#" text))
            (setq line text)))))
    line))

(defun iac-input--previous-code-indent ()
  "Return indentation of the previous nonblank, noncomment line."
  (let ((indent 0)
        (found nil))
    (save-excursion
      (while (and (not found) (> (line-number-at-pos) 1))
        (forward-line -1)
        (let ((text (string-trim-left
                     (buffer-substring-no-properties
                      (line-beginning-position)
                      (line-end-position)))))
          (unless (or (string-empty-p text) (string-prefix-p "#" text))
            (setq indent (current-indentation))
            (setq found t)))))
    indent))

(defun iac-input-indent-line ()
  "Indent the current IAC/SPARTA input line."
  (interactive)
  (let* ((line (iac-input--current-line-text))
         (prev-line (iac-input--previous-code-line))
         (indent (iac-input--previous-code-indent))
         (pos (- (point-max) (point))))
    (cond
     ((or (string-empty-p line) (string-prefix-p "#" line))
      (setq indent 0))
     ((string-match-p "\\`\\(label\\|jump\\|next\\)\\_>" line)
      (setq indent 0))
     ((iac-input--continuation-line-p line)
      (setq indent (+ indent iac-input-indent-offset)))
     ((and prev-line
           (string-match-p "\\`\\(label\\|variable\\)\\_>" prev-line)
           (not (string-match-p "\\`\\(label\\|jump\\|next\\)\\_>" line)))
      (setq indent (+ indent iac-input-indent-offset))))
    (indent-line-to (max indent 0))
    (when (> (- (point-max) pos) (point))
      (goto-char (- (point-max) pos)))))

;;;###autoload
(define-derived-mode iac-input-mode prog-mode "IAC Input"
  "Major mode for isthmus-ablation-core and SPARTA input files."
  :syntax-table iac-input-mode-syntax-table
  (setq-local comment-start "# ")
  (setq-local comment-end "")
  (setq-local comment-start-skip "#+\\s-*")
  (setq-local indent-line-function #'iac-input-indent-line)
  (setq-local font-lock-defaults '(iac-input-font-lock-keywords nil t)))

;;;###autoload
(add-to-list 'auto-mode-alist '("/in\\.[^/]+\\'" . iac-input-mode))
;;;###autoload
(add-to-list 'auto-mode-alist '("\\.iac\\'" . iac-input-mode))
;;;###autoload
(add-to-list 'auto-mode-alist '("\\.sparta\\'" . iac-input-mode))

(provide 'iac-input-mode)

;;; iac-input-mode.el ends here
