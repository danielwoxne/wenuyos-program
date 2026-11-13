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
lessons = (root / "src/Lessons.cpp").read_text(encoding="utf-8")
manifest = (root / "wenuyos-app.toml").read_text(encoding="utf-8")
integration = (root / "src/SystemIntegration.cpp").read_text(encoding="utf-8")
assert 'wenuy::i18nInit("se.wenuyos.app.schack")' in integration
assert 'explanation(chess::explainMove(app.game.board(),r->result.move))' in gui
assert 'app.analyzeBtn=button(tools,tr("Review last move","Analysera senaste draget"),18,false,"system-search-symbolic");' in gui
assert 'parsed.mistakes[i]=std::min(value,999u);' in lessons
assert 'parsed.successes[i]=std::min(value,999u);' in lessons
assert 'app.lessonSuccesses[app.lessonIndex]<999' in gui
assert 'app.lessonProfile=label(app.lessonBox,"",true,"wenuy-dim");' in gui
assert 'app.undoBtn=button(header,tr("Undo","Ångra"),1);' in gui
assert 'if(app.undoBtn)updateButtonLabel(app.undoBtn,app.training?' in gui
assert 'tr("Restart exercise","Starta om övningen")' in gui
assert 'app.transient=tr("Exercise restarted. Follow the plan from the beginning."' in gui

assert 'app.lessonPly=0;' in gui
assert 'std::to_string((length+1)/2)' in gui
assert 'chess::lessonGoalSv(app.lessonIndex)' in gui
assert 'chess::lessonStepHintEn(app.lessonIndex,app.lessonPly,app.lessonVariation)' in gui
assert 'stopPieceMotion();' in gui

assert 'app.lessonVariation=app.lessonSuccesses[app.lessonIndex]%' in gui
assert 'chess::lessonLineMoveVariant(app.lessonIndex,app.lessonPly,' in gui
assert 'chess::lessonAcceptsMove(app.lessonIndex,app.lessonPly,app.lessonVariation,board,m)' in gui
assert 'chess::lessonStepFeedbackEn(app.lessonIndex,app.lessonPly-1,app.lessonVariation)' in gui

assert 'const char* reply=chess::lessonLineMoveVariant(app.lessonIndex,app.lessonPly,' in gui
assert 'app.lessonPly==chess::lessonLineLength(app.lessonIndex)' in gui
assert 'tr("Opponent replied.","Motspelaren har svarat.")' in gui

assert 'chess::lessonGroupCount()' in gui
assert 'std::istringstream counts(line)' in lessons
assert 'tr("Knowledge profile:","Kunskapsprofil:")' in gui
assert "output<<progress.completed" in lessons
assert 'chess::writeLessonProgress(out,progress);' in gui
assert 'chess::readLessonProgress(input,progress)' in gui
assert 'chess::recommendedLesson(app.lessonMask,app.lessonMistakes,app.lessonSuccesses)' in gui
assert 'case 19:if(app.training)' in gui
assert 'tr("Recommended practice","Rekommenderad övning")' in gui
assert 'tr("Not quite. Try again: check the squares your piece controls, or use the hint."' in gui

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
assert 'version = "0.5.2"' in manifest
assert 'needs_network = true' in manifest

# 0.5.2: richer-looking board without any raster asset, texture, blur or GPU.
# The last-move indicator and legal move overlay must reuse the active palette.
for marker in (
    "An inexpensive contact shadow anchors the vector piece",
    "cairo_translate(c,x+s*.50,y+s*.845);",
    "cairo_scale(c,s*.30,s*.055);",
    "Fine inner rim adds depth to the frame",
    "Theme-aware last-move tint plus corner brackets",
    "rgba(c,pal.accent[0],pal.accent[1],pal.accent[2],.18);",
    "rgba(c,pal.accent[0],pal.accent[1],pal.accent[2],.54);",
):
    assert marker in gui, f"Missing lightweight graphics polish: {marker}"
assert "rgba(c,1,.75,.22,.3)" not in gui, "Last move highlight must follow theme"
assert "rgba(c,.12,.27,.42,.47)" not in gui, "Legal move hint must follow theme"

# 0.5.2: theme-friendly controls and sharper pieces, with no extra assets.
for marker in (
    "A stepped mane and nostril improve the knight silhouette",
    "A neck ring makes the bishop readable on small squares",
    "Shoulder folds add dimension without bitmap shading",
    'Ptr gameSection=section(side,tr("YOUR GAME","DITT PARTI"),"applications-games-symbolic");',
    'Ptr settingsSection=section(side,tr("PLAY SETTINGS","SPELINSTÄLLNINGAR"),"preferences-system-symbolic");',
    'Ptr movesSection=section(side,tr("MOVE HISTORY","DRAGLISTA"),"view-list-symbolic");',
    'Ptr lanSection=section(side,tr("LAN GAME","NÄTVERKSPARTI"),"network-workgroup-symbolic");',
    'g.gtk_widget_add_css_class(tools,"wenuy-toolbar");',
    'g.gtk_widget_add_css_class(app.lessonBox,"wenuy-section");',
    '.wenuy-section-title {font-weight: 700;',
    'alpha(currentColor, 0.13)',
    'g.gtk_window_set_title(app.window,tr("Wenuy Chess","Wenuy Schack"));',
    'app.brand=g.gtk_label_new(tr("♞  WENUY CHESS","♞  WENUY SCHACK"));',
):
    assert marker in gui, f"Theme UI or vector drawing missing: {marker}"
assert "border-radius: 16px" not in gui and "border-radius: 9px" not in gui

# Match the approved visual target without raster assets or hard-coded widget colours.
# The school card must remain visible in normal play, showing genuine saved progress.
for marker in (
    "M(Ptr,gtk_image_new_from_icon_name,(const char*))",
    "M(Ptr,gtk_icon_theme_get_for_display,(Ptr))",
    "M(int,gtk_icon_theme_has_icon,(Ptr,const char*))",
    "M(Ptr,gtk_progress_bar_new,())",
    "M(void,gtk_progress_bar_set_fraction,(Ptr,double))",
    "M(void,gtk_progress_bar_set_show_text,(Ptr,int))",
    "M(void,gtk_progress_bar_set_text,(Ptr,const char*))",
    'g.gtk_widget_add_css_class(card,"card");',
    'g.gtk_widget_add_css_class(app.lessonBox,"card");',
    "Ptr themeIcon(const char* iconName)",
    "g.gtk_icon_theme_has_icon(theme,iconName)",
    "g.gtk_button_set_child(btn,content);",
    'button(header,tr("New game","Nytt parti"),0,true,"document-new-symbolic");',
    'sectionHeading(app.lessonBox,tr("CHESS SCHOOL","SCHACKSKOLAN"),"applications-education-symbolic");',
    "app.lessonBar=g.gtk_progress_bar_new();",
    "g.gtk_progress_bar_set_fraction(app.lessonBar,",
    'app.lessonToggleBtn=button(app.lessonBox,tr("Exit chess school","Avsluta schackskolan"),6);',
    'tr("Start chess school","Starta schackskolan")',
    'g.gtk_widget_set_visible(app.lessonButtons,app.training?1:0);',
    'g.gtk_widget_set_visible(app.lessonRecommendBtn,app.training?1:0);',
    'tr("Learn step by step","Lär dig steg för steg")',
):
    assert marker in gui, f"Chess concept-image UI element missing: {marker}"
assert 'g.gtk_widget_set_visible(app.lessonBox,app.training?1:0);' not in gui, (
    "The chess school preview should be visible during ordinary games"
)
assert "GtkProgressBar" not in manifest, "No new runtime dependency should be added"

# Dynamic labels must not discard the custom GTK icon+text child.
for marker in (
    "struct ButtonData{int id;Ptr button=nullptr,caption=nullptr;};",
    "void updateButtonLabel(Ptr button,const char* text){",
    "g.gtk_label_set_label(entry->caption,text);",
    "buttonDatas.push_back(std::make_unique<ButtonData>(ButtonData{id,btn,caption}));",
    "updateButtonLabel(app.levelBtn,",
    "updateButtonLabel(app.modeBtn,",
    "updateButtonLabel(app.themeBtn,",
    "uiProbeRequested=true;uiProbeSmall=",
    "int guiProbeTick(void*)",
    "gtk_widget_compute_bounds(app.board,app.window,&r)",
    "chosen(12);probePhase=2;",
    "chosen(28);probePhase=3;",
):
    assert marker in gui, f"Missing real GTK regression or icon persistence: {marker}"
assert gui.count("std::vector<std::unique_ptr<ButtonData>> buttonDatas;")==1
assert 'g.gtk_button_set_label(app.modeBtn,' not in gui
assert 'g.gtk_button_set_label(app.levelBtn,' not in gui
assert 'g.gtk_button_set_label(app.lessonToggleBtn,' not in gui

# 0.5.2: a changing status message must never wrap and resize the board.
# These are structure checks; actual pixel stability still requires a GTK test.
for marker in (
    "M(void,gtk_label_set_ellipsize,(Ptr,int))",
    "M(void,gtk_label_set_single_line_mode,(Ptr,int))",
    "g.gtk_label_set_wrap(app.boardSummary,0);",
    "g.gtk_label_set_single_line_mode(app.boardSummary,1);",
    "g.gtk_label_set_ellipsize(app.boardSummary,3);",
    "g.gtk_label_set_max_width_chars(app.boardSummary,36);",
    "g.gtk_widget_set_hexpand(app.boardSummary,1);",
    "g.gtk_widget_set_tooltip_text(app.boardSummary,summary.c_str());",
):
    assert marker in gui, f"Board status may reflow and shake: {marker}"
assert "g.gtk_label_set_wrap(app.boardSummary,1);" not in gui

# 0.5.1: no sideways scroll or cut-off shortcut text in 252 px sidebar.
assert 'M(void,gtk_scrolled_window_set_policy,(Ptr,int,int))' in gui
assert 'g.gtk_scrolled_window_set_policy(app.sidePanel,2,1);' in gui
assert 'g.gtk_scrolled_window_set_policy(scroll,2,1);' in gui
assert 'g.gtk_widget_set_size_request(scroll,-1,80);' in gui
assert 'g.gtk_widget_set_hexpand(side,1);' in gui
assert 'g.gtk_label_set_wrap_mode(l,2);' in gui
assert 'g.gtk_label_set_max_width_chars(l,26);' in gui
assert 'g.gtk_label_set_max_width_chars(app.history,28);' in gui
assert 'Ctrl+H tips, Ctrl+L fokus.' in gui
assert 'Ctrl+H hint, Ctrl+L focus.' in gui
assert 'Drag or click pieces • Ctrl+N new' not in gui
assert 'g.gtk_widget_set_size_request(scroll,240,80)' not in gui

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
    'app.lanHostBtn=button(lanSection,tr("Host LAN game (White)","Bjud in på LAN (vit)"),15);',
    'app.lanJoinBtn=button(lanSection,tr("Join LAN game (Black)","Anslut till LAN (svart)"),16);',
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
# Claude review (12 Nov): the chess-school panel must come before the LAN section, so its task and buttons are
# visible without scrolling, and the first window must fit the screen (1024x600, 1280x720 with the panel).
lesson_at = gui.index("app.lessonBox=g.gtk_box_new(1,9);g.gtk_box_append(side,app.lessonBox);")
settings_at = gui.index('Ptr settingsSection=section(side,tr("PLAY SETTINGS","SPELINSTÄLLNINGAR"),"preferences-system-symbolic");')
moves_at = gui.index('Ptr movesSection=section(side,tr("MOVE HISTORY","DRAGLISTA"),"view-list-symbolic");')
lan_at = gui.index('Ptr lanSection=section(side,tr("LAN GAME","NÄTVERKSPARTI"),"network-workgroup-symbolic");')
status_at = gui.index('app.material=label(gameSection,"",false,"wenuy-dim");')
assert status_at < lesson_at < settings_at < moves_at < lan_at, "Keep the game and lessons ahead of settings and optional LAN"
assert "g.gtk_window_set_default_size(app.window,960,700)" not in gui, "Fixed 960x700 window overflows small screens"
assert "const auto size=firstWindowSize();g.gtk_window_set_default_size(app.window,size.first,size.second);" in gui
import re as _re
fit = gui[gui.index("std::pair<int,int> fitWindow("):]
fit = fit[:fit.index("\n}\n")]
assert "w=std::min(w,std::max(480,screenW-32))" in fit and "h=std::min(h,std::max(400,screenH-112))" in fit
def fit_py(sw, sh):
    w, h = 960, 700
    if sw > 0 and sh > 0:
        w, h = min(w, max(480, sw - 32)), min(h, max(400, sh - 112))
    return w, h
assert fit_py(0, 0) == (960, 700) and fit_py(1920, 1080) == (960, 700)
assert fit_py(1024, 600) == (960, 488) and fit_py(1280, 720) == (960, 608) and fit_py(800, 480) == (768, 400)
print("OK: bilingual launcher, accessible GTK4, responsive board, motion policy and Cairo details")
