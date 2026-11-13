// Svarsytorna – det eleven svarar med: skriva, välja, klicka i figurer och målningar, pricka in,
// dra linjer, dra brickor, rita. Samma ytor används i Öva och i placeringstestet.
#pragma once
#include "Bedom.h"
#include "Data.h"
#include "Rita.h"

#include <gtk/gtk.h>

#include <functional>
#include <string>
#include <vector>

namespace matte {

// En figur att titta på eller svara i. Ritas på ett eget vitt papper så att den syns i både
// ljust och mörkt tema.
//   ""        bara visa
//   klick     markera rutor (staplar, prickar, äpplen) – svaret är antalet
//   valj      klicka på EN sak i en målning eller ikonrad – svaret är dess namn
//   markera   klicka på ALLA rätt saker – svaret är namnen
//   punkt     pricka in ett tal på tallinjen eller en punkt i koordinatsystemet
//   linje     två punkter i koordinatsystemet blir en linje – svaret är k och m
GtkWidget* figurvy(const Data& d, const Json& beskrivning, const std::string& lage = "", double skala = 1.0,
                   std::function<void()> vidKlick = nullptr);
std::vector<std::string> figurvySvar(GtkWidget* vy);     // tomt om eleven inte svarat än
void figurvyRensa(GtkWidget* vy);

extern double FIGURSKALA;                                // 1.5 i lekläget

// Allt som behövs för att svara på en uppgift
struct Svarsyta {
    GtkWidget* widget = nullptr;
    std::function<std::vector<std::string>()> svar;     // tom lista = inget svar än
    std::function<void(const Bedomning&, const std::vector<std::string>&)> visa;   // markera rätt/fel
    GtkWidget* fokus = nullptr;
    std::function<void()> visaFacit;                     // efter andra felet (kryssa, para): visa de rätta
    std::function<bool(const std::string&)> spara;       // rityta: spara teckningen som PNG
    std::function<bool()> tom;                           // rityta: inget ritat än
    std::function<void(const std::vector<std::string>&)> fyll;   // testkroken: ställ in ett svar
};

Svarsyta svarsyta(const Data& d, const Mall& m, const Variant& v, std::function<void(std::vector<std::string>)> vidSvar);

// Visar figuren separat (ovanför svaret) för de typer som inte svarar i själva figuren
bool figurVisasSeparat(const std::string& typ);

}  // namespace matte
