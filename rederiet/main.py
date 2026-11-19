#!/usr/bin/env python3
"""Rederiet — bilingual offline fleet-management game for GTK4/WenuyOS."""
import os
from pathlib import Path

import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, GLib

from engine import Game, PORTS, SHIP_TYPES, MAX_FLEET, distance
from map_widget import WorldMap, VIEWS

APP_ID = 'se.wenuyos.app.rederiet'
SV = (os.environ.get('LANGUAGE') or os.environ.get('LC_ALL') or
      os.environ.get('LANG') or 'sv').lower().startswith('sv')


def T(en, sv):
    return sv if SV else en


SHIP_NAMES = {
    'coaster': ('Coaster', 'Kustfraktare'),
    'freighter': ('Freighter', 'Lastfartyg'),
    'ocean': ('Ocean freighter', 'Oceanfraktare'),
    'container': ('Container ship', 'Containerfartyg'),
}
SHIP_KEYS = list(SHIP_TYPES)
WEATHER_NAMES = {
    'calm': ('Calm sea', 'Lugnt hav'),
    'breeze': ('Breeze', 'Bris'),
    'rough': ('Rough seas', 'Grov sjö'),
    'storm': ('Storm', 'Storm'),
}


ERRORS = {
    'ship_busy': ('This vessel is busy.', 'Det här fartyget är upptaget.'),
    'offer_unavailable': ('Contract is no longer available.', 'Uppdraget är inte tillgängligt.'),
    'already_sailing': ('Vessel is already at sea.', 'Fartyget är redan till sjöss.'),
    'bad_destination': ('Choose a different port.', 'Välj en annan hamn.'),
    'not_enough_fuel': ('Not enough fuel for the voyage.', 'För lite bränsle för resan.'),
    'needs_repair': ('Repair before sailing.', 'Reparera före avfärd.'),
    'at_sea': ('Not possible at sea.', 'Inte möjligt till sjöss.'),
    'refuel_unavailable': ('Full tank or insufficient funds.', 'Full tank eller för lite pengar.'),
    'repair_unavailable': ('Fully repaired or insufficient funds.', 'Helt reparerat eller för lite pengar.'),
    'cargo_capacity': ('Not enough cargo capacity.', 'För lite lastutrymme.'),
    'cargo_route_mismatch': ('Only contracts for the same destination may share a voyage.',
                             'Endast frakter till samma hamn kan följa med på resan.'),
    'bad_ship_type': ('Unknown vessel type.', 'Okänd fartygstyp.'),
    'fleet_full': ('Fleet limit reached.', 'Flottan har nått maxstorleken.'),
    'not_enough_money': ('Not enough funds.', 'För lite pengar.'),
    'last_ship': ('The final vessel cannot be sold.', 'Det sista fartyget kan inte säljas.'),
    'unknown_ship': ('Vessel not found.', 'Fartyget hittades inte.'),
}


def translated_log(line):
    if not SV:
        return line
    line = line.replace('Day ', 'Dag ', 1)
    for a, b in (
        ('cargo loaded for', 'lastad för'), ('sailed for', 'avseglade mot'),
        ('arrived in', 'anlände till'), ('cargo delivered', 'last levererad'),
        ('fuel purchased', 'bränsle köpt'), ('ship repaired', 'fartyg reparerat'),
        ('ship purchased', 'fartyg köpt'), ('ship sold', 'fartyg sålt'),
        ('Company bankrupt', 'Rederiet i konkurs'),
        ('storm delayed voyage', 'storm försenade resan'),
        ('harbour dues', 'hamnavgift'),
        ('rival delivered cargo in', 'konkurrent levererade gods i'),
        ('(late)', '(för sent)'), ('TEU', 'lastenheter'),
        ('Cargo loaded', 'Lasten ombord'), ('Cargo delivered', 'Last levererad'),
        ('Sailed for', 'Avseglade mot'), ('Arrived in', 'Anlände till'),
    ):
        line = line.replace(a, b)
    return line


class Window(Gtk.ApplicationWindow):
    def __init__(self, app):
        super().__init__(application=app, title=T('The Shipping Company', 'Rederiet'))
        self.set_default_size(1030, 760)
        self.path = Path(GLib.get_user_data_dir()) / APP_ID / 'save.json'
        self._syncing = False
        self._reset_armed = False
        self._fleet_ids = []
        try:
            self.game = Game.load(self.path) if self.path.exists() else Game()
        except (OSError, ValueError, KeyError, TypeError, UnicodeError):
            self.game = Game()

        scroll = Gtk.ScrolledWindow()
        scroll.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        self.set_child(scroll)
        root = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        for side in ('top', 'bottom', 'start', 'end'):
            getattr(root, 'set_margin_' + side)(12)
        scroll.set_child(root)

        title = Gtk.Label(label=T('THE SHIPPING COMPANY', 'REDERIET'), xalign=0)
        title.add_css_class('title-1')
        root.append(title)
        self.status = Gtk.Label(xalign=0, wrap=True)
        root.append(self.status)
        self.finances = Gtk.Label(xalign=0, wrap=True)
        root.append(self.finances)

        fleet_header = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        root.append(fleet_header)
        fleet_header.append(Gtk.Label(label=T('Your fleet', 'Din flotta'), xalign=0, hexpand=True))
        self.ship_select = Gtk.DropDown.new_from_strings(['Nordstjärnan'])
        self.ship_select.set_hexpand(True)
        self.ship_select.connect('notify::selected', self.choose_ship)
        fleet_header.append(self.ship_select)
        self.fleet_info = Gtk.Label(xalign=0, wrap=True)
        root.append(self.fleet_info)
        self.fleet_overview = Gtk.Label(xalign=0, wrap=True, selectable=True)
        root.append(self.fleet_overview)

        map_header = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        root.append(map_header)
        map_header.append(Gtk.Label(label=T('World map', 'Världskarta'), xalign=0, hexpand=True))
        self.view_names = list(VIEWS)
        self.view_select = Gtk.DropDown.new_from_strings([
            T('World', 'Världen'), T('Europe', 'Europa'),
            T('Atlantic', 'Atlanten'), T('Asia–Pacific', 'Asien–Stilla havet'),
        ])
        self.view_select.connect('notify::selected', self.change_view)
        map_header.append(self.view_select)
        self.world_map = WorldMap(self.select_port, self.select_ship_on_map)
        self.world_map.add_css_class('view')
        root.append(self.world_map)
        self.map_description = Gtk.Label(xalign=0, wrap=True)
        root.append(self.map_description)
        self.weather_status = Gtk.Label(xalign=0, wrap=True)
        root.append(self.weather_status)
        self.rivals_info = Gtk.Label(xalign=0, wrap=True, selectable=True)
        root.append(self.rivals_info)
        root.append(Gtk.Separator())

        self.cargo_label = Gtk.Label(xalign=0, wrap=True)
        root.append(self.cargo_label)
        self.travel_label = Gtk.Label(xalign=0, wrap=True)
        root.append(self.travel_label)

        voyage_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        root.append(voyage_row)
        voyage_row.append(Gtk.Label(label=T('Destination:', 'Destination:')))
        self.destination = Gtk.DropDown.new_from_strings(list(PORTS))
        self.destination.set_hexpand(True)
        self.destination.connect('notify::selected', self.destination_changed)
        voyage_row.append(self.destination)
        self.sail_btn = Gtk.Button(label=T('Sail', 'Segla'))
        self.sail_btn.connect('clicked', self.sail)
        voyage_row.append(self.sail_btn)

        actions = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        root.append(actions)
        self.day_btn = Gtk.Button(label=T('Next day', 'Nästa dag'))
        self.day_btn.connect('clicked', self.next_day)
        actions.append(self.day_btn)
        self.fuel_btn = Gtk.Button()
        self.fuel_btn.connect('clicked', self.refuel)
        actions.append(self.fuel_btn)
        self.repair_btn = Gtk.Button()
        self.repair_btn.connect('clicked', self.repair)
        actions.append(self.repair_btn)

        shipyard = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        root.append(Gtk.Label(label=T('Shipyard — buy or sell vessels', 'Varv – köp och sälj fartyg'), xalign=0))
        root.append(shipyard)
        self.kind_select = Gtk.DropDown.new_from_strings([
            T(*SHIP_NAMES[k]) for k in SHIP_KEYS
        ])
        self.kind_select.set_hexpand(True)
        self.kind_select.connect('notify::selected', self.ship_type_changed)
        shipyard.append(self.kind_select)
        self.buy_btn = Gtk.Button(label=T('Buy ship', 'Köp fartyg'))
        self.buy_btn.connect('clicked', self.buy)
        shipyard.append(self.buy_btn)
        self.sell_btn = Gtk.Button(label=T('Sell selected', 'Sälj valt'))
        self.sell_btn.connect('clicked', self.sell)
        shipyard.append(self.sell_btn)
        self.market_info = Gtk.Label(xalign=0, wrap=True)
        root.append(self.market_info)

        root.append(Gtk.Separator())
        root.append(Gtk.Label(label=T('Available cargo contracts', 'Tillgängliga fraktuppdrag'), xalign=0))
        self.contracts = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        root.append(self.contracts)
        root.append(Gtk.Separator())
        root.append(Gtk.Label(label=T('Captain’s log', 'Kaptenens loggbok'), xalign=0))
        self.history = Gtk.Label(xalign=0, yalign=0, wrap=True, selectable=True)
        root.append(self.history)
        self.reset_btn = Gtk.Button(label=T('New company', 'Nytt rederi'))
        self.reset_btn.connect('clicked', self.restart)
        root.append(self.reset_btn)
        self.notice = Gtk.Label(xalign=0, wrap=True)
        root.append(self.notice)

        self.destination.set_selected(PORTS.index('Oslo'))
        self.update()
        self.connect('close-request', self.on_close)

    def change_view(self, *_):
        if hasattr(self, 'world_map'):
            self.world_map.region = self.view_names[self.view_select.get_selected()]
            self.world_map.queue_draw()

    def select_port(self, port):
        self.destination.set_selected(PORTS.index(port))
        self.destination_changed()

    def select_ship_on_map(self, ship_id):
        self.game.select_ship(ship_id)
        self.update()

    def choose_ship(self, *_):
        if self._syncing or not self._fleet_ids:
            return
        index = self.ship_select.get_selected()
        if index < len(self._fleet_ids):
            self.perform(lambda: self.game.select_ship(self._fleet_ids[index]))

    def ship_type_changed(self, *_):
        if hasattr(self, 'market_info'):
            self.update_market()

    def destination_changed(self, *_):
        if not hasattr(self, 'world_map'):
            return
        index = self.destination.get_selected()
        if 0 <= index < len(PORTS):
            self.world_map.selected = PORTS[index]
            self.world_map.queue_draw()
            if hasattr(self, 'map_description'):
                self.update_map_info()

    def update_map_info(self):
        game = self.game
        if game.voyage:
            self.map_description.set_text(
                game.ship.name + ': ' +
                T('Sailing to ', 'Seglar till ') + game.voyage['destination'] +
                T(' · Days remaining: ', ' · Dagar kvar: ') + str(game.voyage['days_left']))
            return
        selected = self.destination.get_selected()
        if selected >= len(PORTS):
            return
        target = PORTS[selected]
        if target == game.port:
            self.map_description.set_text(T('Choose a port on the map or in the list.',
                                            'Välj en hamn på kartan eller i listan.'))
            return
        self.map_description.set_text(
            f'{game.port} → {target}  ·  {game.voyage_days(target)} ' +
            T('days · ', 'dagar · ') + str(game.fuel_needed(target)) +
            T(' fuel units. Schematic map, not for navigation.',
              ' bränsleenheter. Schematisk karta, inte för navigering.'))

    def store(self):
        try:
            self.game.save(self.path)
        except OSError:
            self.notice.set_text(T('Could not save the game.', 'Kunde inte spara spelet.'))

    def perform(self, fn):
        self._reset_armed = False
        self.reset_btn.set_label(T('New company', 'Nytt rederi'))
        try:
            fn()
            self.notice.set_text('')
            self.store()
        except ValueError as exc:
            code = str(exc)
            self.notice.set_text(T(*ERRORS.get(code, (code, code))))
        self.update()

    def sail(self, *_):
        index = self.destination.get_selected()
        if index < len(PORTS):
            self.perform(lambda: self.game.sail(PORTS[index]))

    def next_day(self, *_):
        self.perform(self.game.advance)
        # One 160 ms harbour pulse per turn; no permanent redraw loop.
        self.world_map.port_pulse = True
        self.world_map.queue_draw()
        GLib.timeout_add(160, self.clear_port_pulse)

    def clear_port_pulse(self):
        self.world_map.port_pulse = False
        self.world_map.queue_draw()
        return False

    def refuel(self, *_):
        self.perform(self.game.refuel)

    def repair(self, *_):
        self.perform(self.game.repair)

    def buy(self, *_):
        index = self.kind_select.get_selected()
        if index < len(SHIP_KEYS):
            self.perform(lambda: self.game.buy_ship(SHIP_KEYS[index]))

    def sell(self, *_):
        self.perform(self.game.sell_ship)

    def restart(self, *_):
        if not self._reset_armed:
            self._reset_armed = True
            self.reset_btn.set_label(T('Confirm new game', 'Bekräfta nytt spel'))
            self.notice.set_text(T('Click again to replace your saved company.',
                                   'Klicka igen för att ersätta ditt sparade rederi.'))
            return
        self.game = Game()
        self._reset_armed = False
        self.reset_btn.set_label(T('New company', 'Nytt rederi'))
        self.notice.set_text('')
        self.store()
        self.update()

    def accept(self, _button, contract_id):
        self.perform(lambda: self.game.accept(contract_id))

    def on_close(self, *_):
        self.store()
        return False

    def update_market(self):
        index = self.kind_select.get_selected()
        if index >= len(SHIP_KEYS):
            return
        kind = SHIP_KEYS[index]
        spec = SHIP_TYPES[kind]
        self.market_info.set_text(
            T(*SHIP_NAMES[kind]) + f': {spec.price:,} kr  ·  {spec.capacity} ' +
            T('cargo slots · ', 'lastplatser · ') + f'{spec.tank} ' +
            T('fuel units · crew ', 'bränsleenheter · besättning ') + f'{spec.crew} ' +
            T('kr/day. ', 'kr/dag. ') +
            T('Sell selected for ', 'Sälj valt för ') +
            f'{self.game.sale_quote(self.game.ship):,} kr')

    def update(self):
        g = self.game
        selected_ship = g.ship
        self.status.set_text(T(
            f'Day {g.day}  ·  Cash {g.money:,} kr  ·  Company value {g.company_value():,} kr  ·  '
            f'Deliveries {g.delivered}  ·  Reputation {g.reputation}/100',
            f'Dag {g.day}  ·  Kassa {g.money:,} kr  ·  Rederivärde {g.company_value():,} kr  ·  '
            f'Leveranser {g.delivered}  ·  Rykte {g.reputation}/100'))
        self.finances.set_text(T(
            f'Income {g.income:,} kr · Expenses {g.expenses:,} kr · '
            f'Late deliveries {g.late} · Vessels {len(g.ships)}/{MAX_FLEET}',
            f'Intäkter {g.income:,} kr · Utgifter {g.expenses:,} kr · '
            f'Sena leveranser {g.late} · Fartyg {len(g.ships)}/{MAX_FLEET}'))

        ids = [ship.id for ship in g.ships]
        if ids != self._fleet_ids:
            self._syncing = True
            self.ship_select.set_model(Gtk.StringList.new([
                f'{s.name} · {T(*SHIP_NAMES[s.kind])}' for s in g.ships
            ]))
            self._fleet_ids = ids
            self._syncing = False
        selected_index = ids.index(g.selected_ship_id)
        self._syncing = True
        self.ship_select.set_selected(selected_index)
        self._syncing = False

        ship_lines = []
        for s in g.ships:
            location = s.voyage['destination'] if s.voyage else s.port
            state = T('at sea to ', 'till sjöss mot ') if s.voyage else T('in ', 'i ')
            ship_lines.append(('● ' if s.id == g.selected_ship_id else '• ') +
                              f'{s.name}: {state}{location} · {s.used_capacity}/{s.capacity} ' +
                              T('cargo slots', 'lastplatser'))
        self.fleet_overview.set_text('\n'.join(ship_lines))
        self.fleet_info.set_text(
            f'{selected_ship.name}  ·  {T(*SHIP_NAMES[selected_ship.kind])}  ·  '
            f'{g.fuel}/{selected_ship.specs.tank} ' + T('fuel', 'bränsle') +
            f'  ·  {g.condition}% ' + T('condition', 'skick') +
            f'  ·  {selected_ship.used_capacity}/{selected_ship.capacity} ' +
            T('cargo slots', 'lastplatser'))

        if selected_ship.cargo:
            cargo = '\n'.join(f'{c.origin} → {c.destination} · {c.size} ' +
                              T('slots', 'platser') + f' · {c.pay:,} kr · ' +
                              T('by day ', 'senast dag ') + str(c.deadline)
                              for c in selected_ship.cargo)
            self.cargo_label.set_text(T('Cargo on selected ship:\n', 'Last på valt fartyg:\n') + cargo)
        else:
            self.cargo_label.set_text(T('Cargo on selected ship: none', 'Last på valt fartyg: ingen'))
        self.travel_label.set_text((
            T('At sea to ', 'Till sjöss mot ') + g.voyage['destination'] +
            T(' · Days remaining: ', ' · Dagar kvar: ') + str(g.voyage['days_left'])
        ) if g.voyage else T('In port: ', 'I hamn: ') + g.port)

        while child := self.contracts.get_first_child():
            self.contracts.remove(child)
        if g.voyage:
            self.contracts.append(Gtk.Label(
                label=T('Contracts available while in port.', 'Uppdrag finns när fartyget är i hamn.'),
                xalign=0))
        else:
            for offer in g.offers:
                row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
                row.append(Gtk.Label(
                    label=f'{offer.origin} → {offer.destination}  ·  {offer.size} ' +
                    T('slots', 'platser') + f'  ·  {offer.pay:,} kr  ·  ' +
                    T('by day ', 'senast dag ') + str(offer.deadline),
                    xalign=0, hexpand=True, wrap=True))
                btn = Gtk.Button(label=T('Accept', 'Acceptera'))
                btn.set_sensitive(selected_ship.used_capacity + offer.size <= selected_ship.capacity and
                                  (not selected_ship.cargo or
                                   selected_ship.cargo[0].destination == offer.destination))
                btn.connect('clicked', self.accept, offer.id)
                row.append(btn)
                self.contracts.append(row)

        self.sail_btn.set_sensitive(g.voyage is None)
        self.fuel_btn.set_label(T('Refuel', 'Tanka') + f' ({g.refuel_quote():,} kr)')
        self.fuel_btn.set_sensitive(g.voyage is None and g.fuel < selected_ship.specs.tank)
        self.repair_btn.set_label(T('Repair', 'Reparera') + f' ({g.repair_quote():,} kr)')
        self.repair_btn.set_sensitive(g.voyage is None and g.condition < 100)
        self.buy_btn.set_sensitive(g.voyage is None and len(g.ships) < MAX_FLEET)
        self.sell_btn.set_sensitive(len(g.ships) > 1 and not g.voyage and not selected_ship.cargo)
        self.history.set_text('\n'.join(map(translated_log, g.log[:12])) or
                              T('Start your first voyage!', 'Börja din första resa!'))
        self.update_market()
        w = g.weather()
        self.weather_status.set_text(
            T('Sea conditions: ', 'Väder till havs: ') + T(*WEATHER_NAMES[w.key]) +
            T(' · Storm delays: ', ' · Stormförseningar: ') + str(g.weather_delays) +
            T(' · Port activity: ', ' · Hamnaktivitet: ') +
            str(g.port_activity()) + '/3' +
            T(' · Harbour dues: ', ' · Hamnavgifter: ') + f'{g.port_fees:,} kr')
        rivals = [T('Other shipping companies (● your ships, ◇ rivals):',
                    'Andra rederier (● dina fartyg, ◇ konkurrenter):')]
        for rival in g.rivals:
            where = (T('sailing to ', 'seglar mot ') + rival.destination) if rival.destination else                     (T('in ', 'i ') + rival.port)
            rivals.append(f'◇ {rival.name}: {where} · ' +
                          T('deliveries ', 'leveranser ') + str(rival.deliveries))
        rivals.append(T('Freight market at your port: ', 'Fraktmarknad i din hamn: ') +
                      str(g.market_index()) + '%')
        self.rivals_info.set_text('\n'.join(rivals))
        self.world_map.game = g
        self.world_map.queue_draw()
        self.update_map_info()


class App(Gtk.Application):
    def __init__(self):
        super().__init__(application_id=APP_ID)

    def do_activate(self):
        Window(self).present()


if __name__ == '__main__':
    App().run()
