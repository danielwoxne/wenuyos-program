# Versionshistorik

## 0.5.0 — inventarium, utrustning, dialogval och syntetiserat ljud

- Infört två material (kristallskärvor och elementessens), egen tillverkning,
  två permanenta utrustningsplatser och faktisk effekt i strid och uthållighet.
- Utrustningsdetaljer syns på spelaren i både 2D och 3D.
- Första dialogsystemet med två val; avstå eller be om hjälp.
- Valfri kortvarig ljudmotor via SDL2, inga upphovsrättsskyddade ljudresurser.
- Nya inventarie- och utrustningsvyer, svenska och engelska gränssnittstexter.
- Validerad sparfilstilläggspost `AETHERIA_EXTRA_5` efter `AETHERIA_EXTRA_4`,
  migration från 0.1–0.4 bevarar progresstallen och krediterar material.
- Headless modelltester för tillverkning, bonusar, dialog, loot och korrupta sparfiler.



## 0.4.0 — interaktion och strid (2026-11-10)

- Riktiga interaktiva invånare (Arin och Sera) istället för rent dekorativa.
- Dialogfönster med svenska/engelska texter, paus i simuleringen under samtal.
- Sidouppdrag med sex insamlingsbara örter, två drycker och XP som belöning.
- Tillfälligt återställande via läkare, högst en gång per sparad spelomgång.
- Fiender: patrullering, upptäckt, förföljelse, återgång, återhämtning,
  tidsstyrd attackvarning och möjlighet att undvika skada.
- Brännskada, enklare träffreaktioner och fiende-HUD.
- Nya markeringar på 2D- och 3D-kartan, dubbel språkversion.
- Validerat sparformat med valfri extension och läsning av äldre versioner.
- Utökade modelltester för uppdrag, dialog, sparning och strid.

# Ändringshistorik

## 0.3.0 – 2026-11-10
- Introducerade delad miljösampling (`Environment.h`) och trädkollision.
- Enkel lutningsbegränsning/segmenttest för rörelse; även fiender följer reglerna.
- Procedurbyggda kroppar med animation från rörelsesträcka, kappa och vingar.
- Dag-/nattcykel, ny diffus belysning, fjärilar, eldflugor, två dekorativa bybor.
- Äldre sparfiler kan läsas; vid överlapp med nytt träd omplaceras spelaren.
- Modelltester, provskärmbilder, låg-/högdetaljsmoke-test i mjukvarurenderare.

# Ändringslogg

## 0.2.0 — 2026-11-10

- Bytte utvecklingsnamn från Wenuy Legends till Aetheria.
- Introducerade en riktig perspektivprojekterad 3D-renderare i SDL2,
  procedurgenererade 3D-höjder och förenklade miljö- och karaktärsmodeller.
- Kamerarelativ rörelse, Z/C för rotation, V för att växla 2D/3D.
- F2 för geometriavstånd, F hopp och G glidning (prototyp).
- Behöll sparformatet för modellversion 1; migrerar tidigare sparfil.
- Utökade enhetstester och dokumentation, licensnotiser och testplan.

## 0.1.0 — 2026-11-10

- Första spelbara top-down actionrollspelprototypen med elementstrider,
  80 × 80 värld, 9 kristaller, 14 fiender, altare, karta och sparning.
