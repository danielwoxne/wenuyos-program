# Aetheria + WenuyOS-biblioteken – första integreringen

Beroende av PR #13 (Aetheria 0.5.0-referens + UI); planen ligger i [ärende #14](https://github.com/danielwoxne/wenuyos-program/issues/14).
Den här grenen kommer **inte** från en separat spelprototyp. 0.6.0-källan måste återföras innan sammanslagning.

- WenuyGFX 0.9: **valbar** Z-buffrad CPU-rendering av Aetherias **samma** världsgeometri (F3 eller `--wenuygfx`). SDK-biblioteket länkas från WenuyOS; ingen motor-kopia.
- Ursprungligt SDL-renderingsläge förblir standard och fallback. 2D, gameplay, HUD, ljud, uppdrag, sparformat förblir orörda.
- 2 GB: låg renderingupplösning och ingen GPU-initiering vid mjukvaruläge, med ett fast minnestak per bild.
- Datatyper för externa bildformat läggs till i nästa avgränsade steg, först efter att grafikregressionerna provats. Ingen kopiering av Datatyper.
- Portar genom D-Bus skjuts upp till separat lösning för SDL-applikationer (nuvarande `WenuyPort` förutsätter GApplication).

Testplan: CMake mot WenuyGFX 0.9, CTest för exporten och CPU-bild, jämförelse av HUD/uppdrag/sparfiler, faktisk WenuyOS-verifiering och 30 min fysisk 2 GB/soft.
