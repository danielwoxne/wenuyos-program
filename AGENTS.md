# WenuyOS-program – instruktioner för alla AI-assistenter

Samma regler som WenuyOS: läs **AGENTS.md och docs/ARBETSORDNING.md i `danielwoxne/New-wenuyos`** (avsnitt 10 gäller
just det här förrådet) och `docs/PROGRAM-FOR-WENUYOS.md` där. Kort:

- En mapp per program (`<program>/`, små bokstäver). Rör bara det program uppgiften gäller.
- Gren `ai/<vem>/<program>`, PR, kontrollen **Program** ska vara grön. **Bara Daniel godkänner.**
- Släpp = taggen `<program>-v<version>` på `main`, först efter Daniels godkännande. Inga `.wapp` eller byggda filer i förrådet.
- Svenska och engelska, 2 GB-golvet, inget nätverk utan att användaren godkänner, inga nycklar eller tokens i förrådet.
