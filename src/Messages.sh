#! /bin/sh
$EXTRACTRC *.rc *.ui *.kcfg >> rc.cpp
for MAP_FILE in maps/*.map; do
    echo "i18nc(\"Game board name\", \"$(head -1 "$MAP_FILE")\");" >> maps.cpp;
done
$XGETTEXT *.cpp -o $podir/kdominate.pot
