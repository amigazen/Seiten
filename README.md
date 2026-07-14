# Seiten

This is **Seiten**, a Bluesky / AT Protocol client for Amiga — a native ReAction
application with a thin CLI, built on [amiatp.library](https://github.com/amigazen/amiatp/)
(which uses [amihttp.library](https://github.com/amigazen/amihttp/) for HTTPS).

Unlike AWeb, Seiten is a **net new** amigazen project: there is no classic Amiga
Bluesky client being revived.  The name *Seiten* nods to Zen Buddhist *satori*
(insight / awakening) — a quiet companion to the amigazen haiku — while remaining
a practical Amiga word for a face of the timeline.

## [amigazen project](http://www.amigazen.com)

*A web, suddenly*

*Forty years meditation*

*Minds awaken, free*

**amigazen project** is using modern software development tools and methods to update and rerelease classic Amiga open source software. Projects include a new AWeb, a new Amiga Python 2, and the ToolKit project - a universal SDK for Amiga.

Key to the amigazen project approach is ensuring every project can be built with the same common set of development tools and configurations, so the ToolKit project was created to provide a standard configuration for Amiga development. All *amigazen project* releases will be guaranteed to build against the ToolKit standard so that anyone can download and begin contributing straightaway without having to tailor the toolchain for their own setup.

Seiten is an original work of the amigazen project. This software is redistributed on terms described in the documentation, particularly the file LICENSE.md

The amigazen project philosophy philosophy is based on openness:

*Open* to anyone and everyone	- *Open* source and free for all	- *Open* your mind and create!

PRs for all projects are gratefully received at [GitHub](https://github.com/amigazen/). While the focus now is on classic 68k software, it is intended that all amigazen project releases can be ported to other Amiga-like systems including AROS and MorphOS where feasible.

## About Seiten

Seiten is a **native Bluesky client** for classic AmigaOS 3.2 and AmigaOS 4.1.
It does not embed a web browser: AT Protocol XRPC, JWT session handling, and
Lexicon helpers live in `amiatp.library`; Seiten owns the engine, prefs, and UI.

**v0.2** (current) defaults to a ReAction GUI with Aeronaut-style chrome.  Pass
CLI command flags to use the same engine from the shell.  Protocol regression
belongs in **AtpTest** (`amiatp` `SDK/Examples`), not in Seiten itself.

Use a Bluesky **app password**, not your account password.

## Requirements

- AmigaOS 3.2+ (or 4.1)
- `amiatp.library` on `LIBS:`
- `amihttp.library` (+ AmiSSL or AmiTLS) on `LIBS:` — opened by amiatp
- ReAction (`lib:reaction.lib` at link time; class libraries on `LIBS:`)
- `PROGDIR:cacert.pem` (or `CAFILE=`) for HTTPS when using AmiTLS

## Features (v0.2)

| Area | Support | Notes |
|------|---------|-------|
| Login / logout / whoami | ✅ | Via amiatp session APIs |
| Home timeline | ✅ | Listbrowser; cursor paging near VProp bottom |
| Compose post | ✅ | New post and reply compose (reply prefixes `@handle`) |
| Profile / resolve | ✅ | CLI and selected GUI paths |
| UTF-8 display | ✅ | AWeb-style fold to Amiga charset (`utf8fold.c`) |
| Avatars / embed thumbs | ✅ | Lazy CDN fetch via `AtpDownloadUrl` + `bitmap.image` |
| Timeline chrome | ✅ | Zebra pens, horizontal separators, wrap (no AutoFit) |
| Like / repost Lexicon | ❌ | Speedbar stubs until amiatp wires records |
| Mentions / Feeds / Search | ❌ | Sidebar placeholders |
| Session save to ENVARC | ❌ | Planned for v0.3 |
| ARexx | ❌ | Later |

### GUI (default)

```
Seiten
Seiten GUI HANDLE=you.bsky.social PASSWORD=xxxx-xxxx-xxxx-xxxx CAFILE=PROGDIR:cacert.pem
```

- Top speedbar: Post, Reply, Star, Repost, Refresh (acts on the selected post)
- Left sidebar: Home / Mentions / Feeds / Search + Profile / Login
- Select a post, then Reply (compose prefills `@handle`) or Refresh the feed

### CLI

```
Seiten LOGIN HANDLE=you.bsky.social PASSWORD=xxxx-xxxx-xxxx-xxxx CAFILE=PROGDIR:cacert.pem TIMELINE LIMIT=10 VERBOSE
Seiten RESOLVE HANDLE=amigazen.bsky.social CAFILE=PROGDIR:cacert.pem VERBOSE
Seiten LOGIN HANDLE=... PASSWORD=... POST TEXT="Hello from Amiga" VERBOSE
```

### Protocol tests

Use **AtpTest** in the amiatp `SDK/Examples` tree for library regression — not Seiten.

## Architecture

| Piece | Role |
|-------|------|
| `engine.c` | Owns `AtpConnection`; shared by CLI and GUI |
| `gui_*.c` | `window.class`, listbrowser timeline, login / compose / speedbars |
| `utf8fold.c` | UTF-8 → Latin-1 display fold |
| `imgload.c` | Thumbnail download + BOOPSI bitmap images |
| `cli.c` | ReadArgs; default path opens the GUI |
| amiatp.library | XRPC, JWT session, Lexicon helpers |
| amihttp.library | HTTP/1.1 + TLS (opened inside amiatp) |

## Build

On Amiga with SAS/C against the ToolKit standard:

```
cd Source/seiten
smake
```

### Prerequisites / dependencies

- SAS/C
- NDK 3.2 / ReAction headers and link libraries
- Built and installed `amiatp.library` (and its amihttp / TLS stack on `LIBS:`)

See [ROADMAP.md](ROADMAP.md) for v0.3 and later plans.

## Roadmap (summary)

### v0.1

- Engine + CLI: LOGIN, LOGOUT, WHOAMI, TIMELINE, PROFILE, POST, RESOLVE

### v0.2 (current)

- ReAction GUI with Aeronaut-style chrome
- Infinite scroll, zebra rows, embed/avatar thumbs
- Select post → Reply / Star / Repost from the top speedbar (Lexicon like/repost incomplete)

### v0.3

- Optional save/load of refresh token under `ENVARC:Seiten/`
- Multi-image gallery / full-size viewer
- Author feed view

### Later

- Notifications
- ARexx port
- Full Lexicon reply / like / repost via amiatp

## Contact

- At GitHub https://github.com/amigazen/seiten/
- on the web at http://www.amigazen.com/ (Amiga browser compatible)
- or email aweb@amigazen.com

## Acknowledgements

*Amiga* is a trademark of **Amiga Inc**.

Bluesky and the AT Protocol are trademarks of their respective owners; Seiten is an independent client and is not affiliated with Bluesky Social PBC.
