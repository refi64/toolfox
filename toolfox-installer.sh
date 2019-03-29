#!/usr/bin/bash

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

error() {
  echo "$@"
  exit 1
}

[[ $HOSTNAME == toolbox ]] || error 'This should be run from inside the toolbox!'

[[ -d _build ]] || meson _build --prefix="$HOME/.local/share/toolfox"
ninja -C _build install

for shell in bash zsh; do
  rc=$HOME/.${shell}rc
  if [[ -f $rc ]] && ! grep -q toolfox $rc; then
    echo >> $rc
    echo '# Added by toolfox-installer.sh' >> $rc
    echo 'export PATH="$PATH:$HOME/.local/share/toolfox/bin:$HOME/.local/share/toolfox/exports/bin"' >> $rc
    echo "eval \"\$(toolfox shellcode $shell)\"" >> $rc
  fi
done
