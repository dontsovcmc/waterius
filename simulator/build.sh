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

# Штамп сборки - зачем он нужен, написано в src/sw.js. Считается по всему, что
# уезжает на сайт, кроме двух файлов, куда его же и подставляем ниже.
# Пути относительные: штамп обязан меняться от содержимого, а не от того, в
# какой папке собирали.
stamp=$(cd "$dist" && find . -type f ! -path ./sw.js ! -path ./index.html \
    -exec shasum {} + | sort -k2 | shasum | cut -c1-12)

sed "s/var BUILD = 'dev';/var BUILD = '$stamp';/" "$dist/sw.js" > "$dist/sw.tmp"
mv "$dist/sw.tmp" "$dist/sw.js"

sed "s|\"/sim/\([^\"?]*\)\"|\"/sim/\1?v=$stamp\"|g" "$dist/index.html" > "$dist/index.tmp"
mv "$dist/index.tmp" "$dist/index.html"

# Молча не подставившийся штамп - ровно та беда, от которой он и заведён
grep -q "var BUILD = '$stamp';" "$dist/sw.js" || { echo "штамп не доехал до sw.js"; exit 1; }
if grep -q '"/sim/[^"?]*"' "$dist/index.html"; then
    echo "в index.html остались ссылки на /sim/ без ?v="
    exit 1
fi

echo "готово: $dist (сборка $stamp)"
echo "локально: cd $dist && python3 -m http.server 8080  ->  http://localhost:8080/"
