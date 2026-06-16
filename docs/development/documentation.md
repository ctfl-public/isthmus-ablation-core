# Documentation

Documentation is part of the development loop. Whenever a command or behavior
is added or edited, update the relevant Markdown page in `docs/` in the same
change.

## Source Format

The manual source is plain Markdown. Command pages live in:

```text
docs/commands/
```

Concept pages live in:

```text
docs/concepts/
```

Examples live in:

```text
docs/examples/
```

Development notes live in:

```text
docs/development/
```

The web documentation layout is described by:

```text
mkdocs.yml
```

The intended web-docs stack is MkDocs Material because it is modern, searchable,
widely used, and still keeps every page as readable Markdown.

## PDF Manual

The repository also has a local PDF builder:

```bash
make docs
```

It writes:

```text
docs/isthmus-ablation-core-manual.pdf
```

The lower-level builder is:

```bash
python3 tools/build-docs-pdf.py
```

This PDF builder is intentionally lightweight. It is a dependable review
artifact for local development, while MkDocs Material remains the richer web
documentation path.

## Update Rule

When functionality changes:

- update the command reference for syntax changes;
- update concepts if the architecture or verification model changes;
- update examples when user-facing input files change;
- rebuild the PDF manual before handing the change back.
