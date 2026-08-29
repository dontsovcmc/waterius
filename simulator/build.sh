#!/bin/sh
# Собирает статику симулятора в dist/.
#
# Страницы прошивки копируются побайтово: симулятор их не правит,
# иначе теряется весь смысл - смотреть ровно то, что увидит пользователь.
set -e

here=$(cd "$(dirname "$0")" && pwd)
dist="$here/dist"
data="$here/../ESP8266/data"

rm -rf "$dist"
mkdir -p "$dist/fw" "$dist/sim/lib"

cp -R "$data/." "$dist/fw/"
diff -r "$data" "$dist/fw" >/dev/null || { echo "копия образа отличается от data/"; exit 1; }

cp "$here/src/index.html" "$dist/index.html"
cp "$here/src/sw.js" "$dist/sw.js"
cp "$here/src/sim/pult.js" "$here/src/sim/pult.css" "$dist/sim/"
cp "$here/src/sim/lib/"*.js "$dist/sim/lib/"

node "$here/gen_from_firmware.js" > "$dist/sim/generated.js"

echo "готово: $dist"
echo "локально: cd $dist && python3 -m http.server 8080  ->  http://localhost:8080/"
