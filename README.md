# WenuyOS-program – många program i ett förråd

Fristående program och spel för WenuyOS (Linux), **en mapp per program**. Ett nytt program kräver alltså inget
nytt förråd – bara en ny mapp.

```
matte/          ett program (data/<id>.desktop, CMakeLists.txt eller main.py …)
klossregn/      ett spel (game.ini eller data/*.desktop med Categories=Game;)
.github/        provet och släppet (workflows/program.yml, wenuyos-pack.sh)
```

## Ett nytt program
1. På WenuyOS: `wenuyos new app "Namn"` (eller `game`), flytta mappen hit som `<program>/` (små bokstäver, bindestreck).
2. Gren `ai/<vem>/<program>`, PR – **Program**-kontrollen provar programmet (bygge, prov, fönstret på 2 GB-profilen
   för C++, paketering för alla).
3. Daniel godkänner → merge.

## Släppa en version
När Daniel godkänt: sätt taggen `<program>-v<version>` på `main` och skicka den:

```bash
git tag matte-v0.3 && git push origin matte-v0.3
```

Kontrollen bygger, signerar med programnyckeln (hemligheten `WENUYOS_PROGRAM_KEY`) och släpper `matte.wapp`.
WenuyOS-datorerna:
- installerar: `wenuyos install github.com/danielwoxne/wenuyos-program#matte`
- får nya versioner med `wenuyos update` (bara släpp vars tagg börjar med `matte-v`)
- ser programmet under *Nytt från WenuyOS* efter en rad i `config/program-catalog.json` i New-wenuyos
  (`"source": "github.com/danielwoxne/wenuyos-program#matte"`)

Byggda filer (`.wapp`, `build/`, `dist/`) läggs aldrig i förrådet – bara i släppen.
