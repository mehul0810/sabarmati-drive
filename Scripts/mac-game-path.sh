# Shared by the package launcher, portable archive and packaged benchmarks.
sabarmati_app="$HOME/Applications/Grand City.app"
if [[ ! -e "$sabarmati_app" && -x "$HOME/Applications/Sabarmati Drive.app/Contents/MacOS/SabarmatiPrototype" ]]; then
    sabarmati_app="$HOME/Applications/Sabarmati Drive.app"
fi
