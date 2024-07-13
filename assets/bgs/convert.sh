#!/bin/sh

for file in *.png; do
    /opt/wonderful/thirdparty/blocksds/core/tools/grit/grit "$file" -ftb -fh! -gTFF00FF -gt -gB8 -mR8 -mLs
done

for file in *.bin; do
    mv -- "$file" "${file%.bin}"
done

mv *.pal *.img *.map ../../nitrofiles/bg