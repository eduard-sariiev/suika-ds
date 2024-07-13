#!/bin/sh

#/opt/wonderful/thirdparty/blocksds/core/tools/grit/grit ball.png -ftb -fh! -gTFF00FF -gt -gB8 -m!

for file in *.png; do
    /opt/wonderful/thirdparty/blocksds/core/tools/grit/grit "$file" -ftb -fh! -gTFF00FF -gt -gB8 -m!
done

for file in *.bin; do
    mv -- "$file" "${file%.bin}"
done

mv *.pal *.img ../../nitrofiles/sprite