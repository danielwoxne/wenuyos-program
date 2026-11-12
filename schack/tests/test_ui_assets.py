#!/usr/bin/env python3
"""Lightweight regression checks for the Wenuy Schack launcher and compact UI.

No GTK display required. Runtime window launch is verified by wenuyos verify in CI.
"""
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

root = Path(sys.argv[1])
desktop = (root / "data/se.wenuyos.app.schack.desktop").read_text(encoding="utf-8")
svg = root / "data/se.wenuyos.app.schack.svg"
gui = (root / "src/RuntimeGtk.cpp").read_text(encoding="utf-8")
manifest = (root / "wenuyos-app.toml").read_text(encoding="utf-8")
integration = (root / "src/SystemIntegration.cpp").read_text(encoding="utf-8")
assert 'wenuy::i18nInit("se.wenuyos.app.schack")' in integration
assert 'explanation(chess::explainMove(app.game.board(),r->result.move))' in gui
assert 'app.analyzeBtn=button(tools,tr("Review last move","Analysera senaste draget"),18);' in gui
assert 'case 18:analyzeLastMove();break;' in gui
assert 'chess::bestMove(before,3,8000)' in gui
assert 'g_idle_add(deliverMoveReview,r);' in gui
assert 'not a proven improvement' in gui
assert 'app.analysisBusy' in gui

assert 'tr("This move gives check.","Draget ger schack.")' in gui
assert 'wenuy::tr(english,swedish)' in integration
assert 'wenuy::loadStyle()' in integration
assert 'wenuy::playSound(gameFinished ? "done" : "step")' in integration
assert 'wenuy::userSetting("motion","on")!="off"' in integration
assert '!wenuy::lowProfile()' in integration
assert 'std::strcmp(perf,"low")==0' in integration


for entry in (
    "Name=Wenuy Chess",
    "Name[sv]=Wenuy Schack",
    "Categories=Game;BoardGame;",
    "Exec=schack",
    "Icon=se.wenuyos.app.schack",
):
    assert entry in desktop, f"Missing launcher metadata: {entry}"

ET.parse(svg)
assert 'version = "0.5.0"' in manifest
assert 'needs_network = true' in manifest
for marker in (
    "case 14:app.focusBoard=!app.focusBoard",
    "g.gtk_scrolled_window_set_child(app.sidePanel,side)",
    'tr("Focus on board","Fokus på brädet")',
    'schack_system::initI18n();',
    'return schack_system::translate(en,sv);',
    'theme();schack_system::applyStyle();refresh();',
    'schack_system::playMoveSound(app.game.result()!="ongoing");',
    'tr("High contrast","Hög kontrast")',
    "app.palette=(app.palette+1)%4",
    'connect(app.window,"notify::width",onWindowWidth,nullptr);',
    'g.gtk_orientable_set_orientation(app.mainBox,compact?1:0);',
    'g.gtk_widget_set_size_request(app.sidePanel,compact?-1:252,compact?168:-1);',
    'g.gtk_orientable_set_orientation(app.toolBar,tiny?1:0);',
    'g.gtk_box_append(boardBox,app.boardSummary);',
    'if(app.boardSummary)',
    'Let GTK/WenuyOS provide application foreground and background colours.',

    "g.gtk_widget_set_size_request(app.board,260,260);",
    "g.gtk_widget_set_size_request(app.sidePanel,252,-1);",
    'app.lanHostBtn=button(side,tr("Host LAN game (White)","Bjud in på LAN (vit)"),15);',
    'app.lanJoinBtn=button(side,tr("Join LAN game (Black)","Anslut till LAN (svart)"),16);',
    'app.lanCodeEntry=g.gtk_entry_new();',
    'tr("Six-digit pairing code","Sexsiffrig parningskod")',
    'tr(" · Pairing code: "," · Parningskod: ")',
    'g_timeout_add(120,pollLan,nullptr);startAI();',
    'if(app.mode==2&&(!app.lanPeer||!app.lanPeer->myTurn()))return;',
    'if(app.lanCancel)app.lanCancel->store(true);',
    "// A two-level, scale-independent plinth adds depth",
    "if(keyLower=='l'){perform(14);return 1;}",
    'bool focusBoard=false,keyboardCursor=false,compactLayout=false,narrowToolbar=false;',
    'if(app.keyboardCursor && sq==app.cursor)',
    'app.keyboardCursor=false;chosen(sq);',
    'app.lanCodeEntry&&g.gtk_widget_has_focus(app.lanCodeEntry)',
    'M(void,gtk_accessible_update_property,(Ptr,int,...))',
    'void updateBoardAccessibility(){',
    'g.gtk_accessible_update_property(app.board,4,label.c_str(),-1);',
    'g.gtk_accessible_update_property(app.board,1,',
    'g.gtk_accessible_update_property(app.lanEntry,4,',
    'g.gtk_accessible_update_property(app.lanCodeEntry,4,',
    '(!app.board || !g.gtk_widget_has_focus(app.board))',
    'refresh();return 1;',
    'constexpr auto duration=std::chrono::milliseconds(160);',
    'g_timeout_add(16,motionFrame,nullptr);',
    'schack_system::allowBoardMotion()',
    'startPieceMotion(m,moving);',
    'startPieceMotion(*match,before.squares[match->from]);',
    'if(!(app.movingPiece&&sq==app.motionTo))piece(',
    'stopPieceMotion();app.game.reset();',
    'case 2:stopPieceMotion();app.flipped=!app.flipped;',
    '// Two masonry seams sharpen the rook silhouette at small sizes.',
    '// Ear and bridle are simple Cairo strokes, not filtered assets.',


    'tr("Selected ","Vald ")',
    'tr(" legal moves"," möjliga drag")',
):
    assert marker in gui, f"Missing UI functionality: {marker}"
print("OK: bilingual launcher, accessible GTK4, responsive board, motion policy and Cairo details")
