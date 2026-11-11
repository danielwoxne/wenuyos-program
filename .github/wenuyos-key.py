#!/usr/bin/env python3
"""Plockar ut programnyckelns block ur hemligheten WENUYOS_PROGRAM_KEY (stdin → stdout) och rättar det som brukar gå
fel när den klistras in från Windows: BOM, CR, mellanslag före/efter raderna, text före eller efter blocket.
Skriver bara formen till stderr (antal rader, om början/slutet finns) – aldrig innehållet."""
import sys
t = sys.stdin.read().replace("﻿", "").replace("\r", "")
lines = [l.strip() for l in t.split("\n")]
B, E = "-----BEGIN PGP PRIVATE KEY BLOCK-----", "-----END PGP PRIVATE KEY BLOCK-----"
has_b = [i for i, l in enumerate(lines) if B in l]
has_e = [i for i, l in enumerate(lines) if E in l]
pub = any("BEGIN PGP PUBLIC KEY BLOCK" in l for l in lines)
print(f"Hemligheten: {len([l for l in lines if l])} rader med text, {len(t)} tecken, början {'finns' if has_b else 'saknas'}, "
      f"slutet {'finns' if has_e else 'saknas'}{', det är den PUBLIKA nyckeln' if pub else ''}", file=sys.stderr)
if not has_b or not has_e or has_e[0] < has_b[0]:
    print("::error::Hemligheten är inte en hel privat nyckel (-----BEGIN PGP PRIVATE KEY BLOCK----- … -----END PGP PRIVATE KEY BLOCK-----)",
          file=sys.stderr)
    sys.exit(1)
if has_b[0] == has_e[0]:                            # allt på en rad (radbrytningarna försvann): dela upp
    mid = lines[has_b[0]].split(B, 1)[1].split(E, 1)[0]
    block = [B, ""] + mid.split() + [E]
else:
    block = lines[has_b[0]:has_e[0] + 1]
    block[0], block[-1] = B, E
    if len(block) == 3 and " " in block[1]:
        block = [B, ""] + block[1].split() + [E]
if len(block) > 1 and block[1]:                     # armor kräver en tom rad efter början (efter ev. rubriker)
    if ":" not in block[1]:
        block.insert(1, "")
print("\n".join(block))
