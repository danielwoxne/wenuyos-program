# Samplingar och instrumentbank – licens och 2 GB (framtida PR)
Plan kopplad till ärende #26 (https://github.com/danielwoxne/wenuyos-program/issues/26).
**Inga externa samplingar har lagts i förrådet eller i en .wapp i denna etapp.** De 16 befintliga instrumenten är originalsyntes och förblir tillgängliga offline.

## Licens och ursprung
- Dokumentera upphovsperson, ursprunglig källa, exakt licensversion, publiceringsdatum, ändringar och tillskrivning.
- Föredra egna inspelningar eller CC0. Andra licenser granskas juridiskt före import. Ingen fil utan tydlig omdistribueringsrätt i Git.
- Behåll licenstext och separat manifest per samplingsbank; registrera även ljud som modifierats.
- Inget nätverk vid programstart. Nätverksåtkomst kräver synligt användarval och Daniels särskilda godkännande.

## Format och ljudmotor
- Läs via WenuyOS Datatyper, inte en ny egen MP3/FLAC-dekoder. Skapa endast vid behov en liten PCM/WAV-cache i användarens datakatalog.
- Ladda bara aktiva instrument. Inledande måltal: högst åtta sampler-röster samtidigt på low-profil; antal måste mätas.
- Minnesbudget att verifiera: 16 MiB per aktiv samplingsbank, högst 32 MiB totalt på low-profil. Inga obegränsade allokeringar i ljudtråden.
- Om bank saknas: tvåspråkigt meddelande och syntetiskt reservinstrument. Projektformatet .wstudio v5 förblir stabilt.

## Villkor innan publicering
1. Separat PR med maskinläsbart licensmanifest och minst ett verifierat instrument.
2. Regresstester av ogiltiga filer, max minne, bank saknas och polyfoni.
3. Oberoende granskning samt ljudtest och 30 minuters 2 GB/soft-prov på faktisk hårdvara.
4. Endast Daniel godkänner publicering och särskilda licens-/nätverksval.