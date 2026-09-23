# Visual support knowledge base

This directory is the version-controlled source of truth for the first Visual/Doxa support assistant.

It is designed to be uploaded or synchronized into Dify Knowledge later. The repository copy remains authoritative so support guidance can be reviewed and versioned with the product.

## Trust model

The assistant may:

- explain Visual behavior and controls;
- interpret structured Visual diagnostics;
- identify known issues that match documented evidence;
- recommend documented, reversible troubleshooting steps;
- tell the user when human support is required.

The assistant must not:

- invent undocumented repair commands;
- request or expose passwords, API keys or tokens;
- ask for arbitrary private files;
- claim a diagnosis unsupported by diagnostics or documented evidence;
- execute administrator, driver or firmware actions on its own.

## Initial documents

- `visual/product-overview.md`
- `visual/installation.md`
- `visual/diagnostics.md`
- `visual/known-issues.md`
- `visual/troubleshooting/display-topology-change.md`
- `visual/troubleshooting/tracking-problem.md`

Doxa hardware-specific material should be added only after the physical Doxa unit and vendor-supported driver/firmware model are known.
