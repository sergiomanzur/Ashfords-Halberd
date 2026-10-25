# Offline identity and menu integration

User-directed scope: remove all online/co-op systems; expose video/audio/QoL
and cheats in both enhancement menus; create an original icon, native launcher
and score; rewrite the README with honest gameplay status. Technical choices
are delegated by the user's earlier instructions.

Implementation responsibilities are separated between offline engine cleanup,
shared menus/cheats, and launcher/music; the parent integrates artwork, host
startup, packaging, documentation and validation. Preserve source attribution
and original retail data requirements.

Validation: full x86 build, component tests, source/data boundary checks,
offline import/source audit, launcher screenshot and transition, gameplay
opening smoke and menu inspection. Full campaign completion remains a separate
unverified acceptance criterion.
