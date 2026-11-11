#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Builds the GitHub review branch locally: NO automatic network/install action.
set -Eeuo pipefail
if [[ ${LANG:-} == sv* ]]; then
  say() { printf '%s\n' "$1"; }
  intro='Detta hämtar källkod från GitHub och kan installera byggberoenden. Fortsätta? [j/N] '
  confirm='Installera den provade, osignerade .wapp-filen? [j/N] '
  absent='WenuyOS SDK/verktyg saknas: installera dev och git först.'
  exists='Projektmappen finns redan. Den skrivs inte över:'
else
  say() { printf '%s\n' "$1"; }
  intro='This downloads source from GitHub and may install build dependencies. Continue? [y/N] '
  confirm='Install this tested, unsigned .wapp file? [y/N] '
  absent='Missing WenuyOS SDK/tools: install dev and git first.'
  exists='The project directory already exists and will not be overwritten:'
fi
command -v git >/dev/null && command -v wenuyos >/dev/null || { say "$absent"; exit 2; }
target="$HOME/Projects/WenuyStudio-0.8.1-hardwaretest"
[[ ! -e "$target" ]] || { say "$exists $target"; exit 3; }
read -r -p "$intro" answer
case "$answer" in j|J|ja|JA|y|Y|yes|YES) ;; *) exit 0;; esac
mkdir -p "$HOME/Projects"
git clone --depth 1 --single-branch --branch ai/chatgpt/musikstudio-080 \
  https://github.com/danielwoxne/wenuyos-program.git "$target"
cd "$target/musikstudio"
wenuyos verify --pack
f="$PWD/dist/musikstudio.wapp"
[[ -s "$f" ]] || { say "No .wapp was created"; exit 4; }
if command -v wenuyos-get >/dev/null; then wenuyos-get --check "$f"; fi
read -r -p "$confirm" answer
case "$answer" in j|J|ja|JA|y|Y|yes|YES) wenuyos install "$f";; *) say "$f";; esac
