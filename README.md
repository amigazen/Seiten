# Seiten

This is **Seiten**, a Bluesky / AT Protocol client for Amiga — a native ReAction
application, built on [amiatp.library](https://github.com/amigazen/amiatp/)

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

Seiten is a **native Bluesky client** for classic Amiga.

Seiten means 'clear sky'.

## Requirements

- Classic Amiga 3.x with ReAction
- `amiatp.library` in `LIBS:`
- `amihttp.library` (+ AmiSSL or AmiTLS) in `LIBS:`

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

## Contact

- At GitHub https://github.com/amigazen/seiten/
- on the web at http://www.amigazen.com/ (Amiga browser compatible)
- or email aweb@amigazen.com

## Acknowledgements

*Amiga* is a trademark of **Amiga Inc**.
