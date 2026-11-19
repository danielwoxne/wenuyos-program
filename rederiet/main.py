#!/usr/bin/env python3
"""WenuyOS Rederiet — offline GTK4 maritime management game."""
import os
from pathlib import Path
import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, GLib
from engine import Game, PORTS, distance
from map_widget import WorldMap, VIEWS

APP_ID = 'se.wenuyos.app.rederiet'
SV = (os.environ.get('LANGUAGE') or os.environ.get('LC_ALL') or os.environ.get('LANG') or 'sv').lower().startswith('sv')


def T(en, sv):
    return sv if SV else en


LOG_MESSAGES = {
    'Cargo loaded': ('Cargo loaded', 'Lasten ombord'),
    'Cargo delivered': ('Cargo delivered', 'Last levererad'),
    'Fuel purchased': ('Fuel purchased', 'Bränsle köpt'),
    'Ship repaired': ('Ship repaired', 'Fartyget reparerat'),
    'Company bankrupt': ('Company bankrupt', 'Rederiet i konkurs'),
}


def translated_log(line):
    if not SV:
        return line
    line = line.replace('Day ', 'Dag ', 1)
    for source, (_en, swedish) in LOG_MESSAGES.items():
        line = line.replace(source, swedish)
    line = line.replace('Sailed for ', 'Avseglade mot ')
    line = line.replace('Arrived in ', 'Anlände till ')
    return line


class Window(Gtk.ApplicationWindow):
    def __init__(self, app):
        super().__init__(application=app, title=T('The Shipping Company', 'Rederiet'))
        self.set_default_size(960, 720)
        self.path = Path(GLib.get_user_data_dir()) / APP_ID / 'save.json'
        try:
            self.game = Game.load(self.path) if self.path.exists() else Game()
        except (OSError, ValueError, KeyError, TypeError):
            self.game = Game()

        scroll = Gtk.ScrolledWindow()
        scroll.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        self.set_child(scroll)
        root = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        for side in ('top', 'bottom', 'start', 'end'):
            getattr(root, 'set_margin_' + side)(12)
        scroll.set_child(root)
        heading = Gtk.Label(label=T('THE SHIPPING COMPANY', 'REDERIET'), xalign=0)
        heading.add_css_class('title-1')
        root.append(heading)
        self.status = Gtk.Label(xalign=0, wrap=True)
        root.append(self.status)

        map_header = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        root.append(map_header)
        map_header.append(Gtk.Label(label=T('World map', 'Världskarta'), xalign=0, hexpand=True))
        self.view_names = list(VIEWS)
        self.view_select = Gtk.DropDown.new_from_strings([
            T('World', 'Världen'), T('Europe', 'Europa'),
            T('Atlantic', 'Atlanten'), T('Asia–Pacific', 'Asien–Stilla havet')
        ])
        self.view_select.connect('notify::selected', self.change_view)
        map_header.append(self.view_select)
        self.world_map = WorldMap(self.select_port)
        self.world_map.add_css_class('view')
        root.append(self.world_map)
        self.map_description = Gtk.Label(xalign=0, wrap=True)
        root.append(self.map_description)

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
        for label, method in [
            (T('Next day', 'Nästa dag'), self.next_day),
            (T('Refuel', 'Tanka'), self.refuel),
            (T('Repair', 'Reparera'), self.repair),
            (T('New company', 'Nytt rederi'), self.restart),
        ]:
            btn = Gtk.Button(label=label)
            btn.connect('clicked', method)
            actions.append(btn)
        self.notice = Gtk.Label(xalign=0, wrap=True)
        root.append(self.notice)
        root.append(Gtk.Label(label=T('Available cargo contracts', 'Tillgängliga fraktuppdrag'), xalign=0))
        self.contracts = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        root.append(self.contracts)
        root.append(Gtk.Separator())
        root.append(Gtk.Label(label=T('Captain’s log', 'Kaptenens loggbok'), xalign=0))
        self.history = Gtk.Label(xalign=0, yalign=0, wrap=True, selectable=True)
        root.append(self.history)
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

    def destination_changed(self, *_):
        if not hasattr(self, 'world_map') or not hasattr(self, 'destination'):
            return
        index = self.destination.get_selected()
        if index >= len(PORTS):
            return
        target = PORTS[index]
        self.world_map.selected = target
        self.world_map.queue_draw()
        if hasattr(self, 'map_description'):
            self.update_map_info()

    def update_map_info(self):
        g = self.game
        if g.voyage:
            self.map_description.set_text(
                T('Route to ', 'Rutt till ') + g.voyage['destination'] +
                T('  ·  Sail days remaining: ', '  ·  Återstående sjödagar: ') +
                str(g.voyage['days_left']))
        else:
            target = PORTS[self.destination.get_selected()]
            if target == g.port:
                self.map_description.set_text(T('Select a port on the map or in the list.',
                                                'Välj en hamn på kartan eller i listan.'))
            else:
                self.map_description.set_text(
                    f'{g.port} → {target}  ·  {distance(g.port, target)} ' +
                    T('days  ·  ', 'dagar  ·  ') + f'{distance(g.port, target) * 3}% ' +
                    T('fuel. Schematic routes, not navigation data.',
                      'bränsle. Schematisk rutt, inte navigationsdata.'))

    def store(self):
        try:
            self.game.save(self.path)
        except OSError:
            self.notice.set_text(T('Could not save game.', 'Kunde inte spara spelet.'))

    def perform(self, fn):
        try:
            fn()
            self.notice.set_text('')
            self.store()
        except ValueError as exc:
            messages = {
                'ship_busy': T('Ship is busy.', 'Fartyget är upptaget.'),
                'offer_unavailable': T('Offer not available.', 'Uppdraget är inte tillgängligt.'),
                'already_sailing': T('Already at sea.', 'Redan till sjöss.'),
                'bad_destination': T('Choose another port.', 'Välj en annan hamn.'),
                'not_enough_fuel': T('Not enough fuel.', 'För lite bränsle.'),
                'needs_repair': T('Repair the ship first.', 'Reparera fartyget först.'),
                'at_sea': T('Not possible at sea.', 'Inte möjligt till sjöss.'),
                'refuel_unavailable': T('Not enough money or tank full.', 'För lite pengar eller full tank.'),
                'repair_unavailable': T('Not enough money or ship repaired.', 'För lite pengar eller helt fartyg.'),
            }
            self.notice.set_text(messages.get(str(exc), str(exc)))
        self.update()

    def sail(self, *_):
        self.perform(lambda: self.game.sail(PORTS[self.destination.get_selected()]))

    def next_day(self, *_):
        self.perform(self.game.advance)

    def refuel(self, *_):
        self.perform(self.game.refuel)

    def repair(self, *_):
        self.perform(self.game.repair)

    def restart(self, *_):
        self.game = Game()
        self.notice.set_text('')
        self.store()
        self.update()

    def accept(self, _button, contract_id):
        self.perform(lambda: self.game.accept(contract_id))

    def on_close(self, *_):
        self.store()
        return False

    def update(self):
        g = self.game
        self.status.set_text(T(
            f'Day {g.day}  •  Cash {g.money:,} kr  •  Fuel {g.fuel}%  •  Condition {g.condition}%  •  Deliveries {g.delivered}',
            f'Dag {g.day}  •  Kassa {g.money:,} kr  •  Bränsle {g.fuel}%  •  Skick {g.condition}%  •  Leveranser {g.delivered}'))
        self.cargo_label.set_text((
            T('Cargo: ', 'Last: ') +
            f'{g.cargo.origin} → {g.cargo.destination}  |  {g.cargo.pay:,} kr  |  ' +
            T('Deadline', 'Sista dag') + f' {g.cargo.deadline}'
        ) if g.cargo else T('Cargo: none', 'Last: ingen'))
        self.travel_label.set_text((
            T('At sea, arriving at ', 'Till sjöss, på väg mot ') +
            f"{g.voyage['destination']} ({g.voyage['days_left']} " +
            T('days left)', 'dagar kvar)')
        ) if g.voyage else T('In port: ', 'I hamn: ') + g.port)
        while child := self.contracts.get_first_child():
            self.contracts.remove(child)
        for offer in g.offers:
            row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
            label = Gtk.Label(
                label=f'{offer.origin} → {offer.destination}  •  {offer.pay:,} kr  •  ' +
                T('by day', 'senast dag') + f' {offer.deadline}',
                xalign=0, hexpand=True, wrap=True)
            row.append(label)
            button = Gtk.Button(label=T('Accept', 'Acceptera'))
            button.connect('clicked', self.accept, offer.id)
            row.append(button)
            self.contracts.append(row)
        self.sail_btn.set_sensitive(g.voyage is None)
        self.history.set_text('\n'.join(map(translated_log, g.log[:12])) or
                              T('Start your first voyage!', 'Börja din första resa!'))
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
