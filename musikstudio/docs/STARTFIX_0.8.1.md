# Musikstudio 0.8.1 – grafikstart och programikon
## Rättelser
- Ny GTK4/libadwaita-huvudklient, native Wayland/GTK och mjukvarurendering; inget XWayland-krav.
- Arrangemang, pianorulle och mixer med WAV/MIDI-import/export, uppspelning och mikrofoninspelning.
- Skalbar SVG-programikon (inte tom) och matchande app-ID i startmenyn.
- Tidigare 0.8-program kunde starta i Xvfb men krävde X11; exakt fel på användarens WenuyOS-maskin är ej bekräftat.
## Prov
- Kör GitHub Program-CI i Debian 13 för kompilering, 4 CTest, GUI-start i 2 GB-profil och paketering.
- Vid fysisk provning kontrollera mikrofon/högtalare, WAV/MIDI, installation och icon cache.
- Ingen sammanslagning/släpp utan Daniels godkännande av sista revisionen.
