# WenuyOS-program – instruktioner för alla AI-assistenter

Samma regler som WenuyOS: läs **AGENTS.md och docs/ARBETSORDNING.md i `danielwoxne/New-wenuyos`** (avsnitt 10 gäller
just det här förrådet) och `docs/PROGRAM-FOR-WENUYOS.md` där. Kort:

- En mapp per program (`<program>/`, små bokstäver). Rör bara det program uppgiften gäller.
- Gren `ai/<vem>/<program>`, PR, kontrollen **Program** ska vara grön (på GitHub eller lokalt i Debian 13, resultatet i
  PR:et). En **annan** AI-tjänst (eller Daniel) granskar och skriver "Granskat och godkänt" – sedan får det slås ihop
  (New-wenuyos #79, Daniel 13 nov 2026). **Daniel godkänner själv** ("godkänn #N"): släpp (taggar), nätverk och
  integritet, och regler (den här filen, `.github/`).
- Släpp = taggen `<program>-v<version>` på `main`, först efter Daniels godkännande. Osignerat: `<program>-v<version>-osignerad`.
  Kan du inte skicka taggar: `git push origin main:slapp/<program>-v<version>` (kontrollen sätter taggen och tar bort grenen). Inga `.wapp` eller byggda filer i förrådet.
- Svenska och engelska, 2 GB-golvet, inget nätverk utan att användaren godkänner, inga nycklar eller tokens i förrådet.
