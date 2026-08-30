# Third-Party Notices

RedEclipseHeadTracking bundles, statically links, or credits the third-party components
listed below. Each remains the property of its authors and is used under its own
licence. Where a licence requires the copyright notice, the conditions and the
disclaimer to accompany a binary distribution, the full text is reproduced here
verbatim, and this file ships at the root of every release ZIP we publish.

Nothing in this repository is derived from, or redistributes any part of,
Red Eclipse.

| Component | Version | Licence | How it ships |
|-----------|---------|---------|--------------|
| Ultimate ASI Loader | v9.7.2 | MIT | Bundled verbatim in the installer ZIP |
| MinHook | v1.3.4 | BSD-2-Clause | Compiled into the `.asi`; licence at `licenses/minhook-LICENSE.txt` in both ZIPs |
| cameraunlock-core | 0f7a63455ddeb91677c9268e88fd35833aa77359 | MIT | Compiled into the `.asi`; licence at `licenses/cameraunlock-core-LICENSE.txt` in both ZIPs |
| OpenTrack | n/a | ISC | Not bundled; UDP protocol interoperability only |

---

## Ultimate ASI Loader

Vendored at `vendor/ultimate-asi-loader/`, shipped in the installer ZIP and used as the
install-time source. Taken from the upstream release asset untouched; the
upstream licence file ships beside it at `vendor/ultimate-asi-loader/LICENSE`.

- Upstream: https://github.com/ThirteenAG/Ultimate-ASI-Loader
- Version: `v9.7.2`
- Commit: `ab722befd52581a34449b603926cfab476e66b05`
- SHA-256: `22fda9c71eaae02460f311bf3441638340ab591586d78f1de213c4819dcb883c`

```
MIT License

Copyright (c) 2023 ThirteenAG

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## MinHook

Source committed at `extern/minhook/` and compiled into the mod's `.asi`. The
committed tree is the authoritative record of exactly what is built.

- Upstream: https://github.com/TsudaKageyu/minhook
- Version: `v1.3.4`
- Verified: every file under `extern/minhook/` hashes identical to the
  upstream `v1.3.4` tag except `src/hook.c`, which carries the change noted
  below.

MinHook carries two copyright holders: Tsuda Kageyu for MinHook itself, and
Vyacheslav Patkov for the Hacker Disassembler Engine that `src/hde/` is built
from. Both notices appear below exactly as upstream ships them.

This copy is modified: `MH_Initialize` uses `GetProcessHeap()` rather than
standing up a private heap with `HeapCreate`, and `MH_Uninitialize` skips the
matching `HeapDestroy`. BSD-2-Clause permits the change; it is recorded here so
the attribution is not mistaken for a claim of an unmodified copy.

```
MinHook - The Minimalistic API Hooking Library for x64/x86
Copyright (C) 2009-2017 Tsuda Kageyu.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

================================================================================
Portions of this software are Copyright (c) 2008-2009, Vyacheslav Patkov.
================================================================================
Hacker Disassembler Engine 32 C
Copyright (c) 2008-2009, Vyacheslav Patkov.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

-------------------------------------------------------------------------------
Hacker Disassembler Engine 64 C
Copyright (c) 2008-2009, Vyacheslav Patkov.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

---

## cameraunlock-core

Git submodule at `cameraunlock-core/`, compiled into the mod's `.asi`. It is our
own code but a different copyright holder from this mod's own LICENSE, so its
notice ships in its own right: verbatim below, and as a file at
`licenses/cameraunlock-core-LICENSE.txt` in both release ZIPs.

- Pinned commit: `0f7a63455ddeb91677c9268e88fd35833aa77359`

```
MIT License

Copyright (c) 2026 CameraUnlock

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## OpenTrack

Not bundled and not linked. This mod implements the OpenTrack UDP pose datagram
layout so that OpenTrack (https://github.com/opentrack/opentrack, ISC licence)
and compatible trackers can drive it. No OpenTrack code, headers or binaries
are copied, linked or redistributed, so its licence triggers no notice
obligation here. It is credited because the wire format is its work.

---

## Red Eclipse

Red Eclipse is free and open-source software, and it is free of charge,
including on Steam. This mod is not affiliated with, endorsed by or sponsored
by the Red Eclipse project.

### What the game is licensed under

- Engine and game source: zlib licence.
  Red Eclipse, Copyright (C) 2009-2025 Quinton Reeves, Lee Salzman,
  Sławomir Błauciak. Built on Tesseract, Copyright (C) 2014-2019 Wouter van
  Oortmerssen, Lee Salzman, Mike Dysart, Robert Pointon, Quinton Reeves and
  Benjamin Segovia; and on Cube Engine 2, Copyright (C) 2001-2019 Wouter van
  Oortmerssen, Lee Salzman, Mike Dysart, Robert Pointon and Quinton Reeves.
- Game content (maps, textures, sounds, models): not covered by the zlib
  licence. Absent an explicit licence it is CC-BY-SA 4.0 or later,
  Copyright (C) 2009-2025 Red Eclipse Team.
- Full terms ship with the game in `doc/license.txt` and `doc/all-licenses.txt`,
  and are published at https://github.com/redeclipse/base.

### What this mod takes from the game

Nothing. No game source, no game content, no game binaries and no PDB are
copied into this repository or into either release ZIP, and none of our code is
derived from the game's. The mod is not a derivative work of Red Eclipse under
the zlib licence, so that licence places no obligation on it, and our own MIT
licence is not a statement about the game's.

The mod reaches the engine entirely by name. At load it hands DbgHelp the
`redeclipse_windows_amd64.pdb` that Red Eclipse itself installs beside the
executable, looks up a dozen of the engine's own public symbols
(`setcammatrix`, `game::recomputecamera`, `hud::drawpointers`, `camera1`,
`cammatrix` and similar) and hooks the addresses it gets back. Those names are
published in the game's own open-source tree. Nothing here was obtained by
decompiling or disassembling the game, and this repository stores no
disassembly, no decompiled function bodies and no byte signatures of game code.
`src/engine_types.h` declares structures whose memory layout matches the
engine's so the mod can read and write those globals in place; it is a layout
description written from the public source, not a copy of it.

### The Red Eclipse Mark Policy

"Red Eclipse", the Red Eclipse emblem and the Red Eclipse logo are marks
governed by the Red Eclipse Mark Policy (`doc/trademark.txt` in the game, and
https://www.redeclipse.net/docs/Trademark_Policy). The policy records the
project as the property of Mr Quinton Reeves as sole proprietor.

This mod uses the name only to say factually which game it works with, which is
the use the policy permits for a product designed to work with Red Eclipse. It
is unofficial and community-made, it claims no association with or endorsement
by the project, and it is not a modified build or redistribution of the game.
If the Red Eclipse project would prefer a different name or presentation, we
will change it on request: contact@redeclipse.net.

### Playing online

Red Eclipse is played mostly online. Head tracking widens what a player can see
without moving their aim, which is an advantage over someone on a plain mouse,
so the README asks players to switch it off on servers where it would not be
welcome. The mod alters no game logic, no hitboxes and no network traffic.
