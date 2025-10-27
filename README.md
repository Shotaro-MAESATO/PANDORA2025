# For PANDORA2025

**ROOT version:** 6.32.06
source /np1a/phanes/opt/root/6.32.06/bin/thisroot.sh

If you have saho account, you can decode the data as follows:

```bash
git clone https://github.com/Shotaro-MAESATO/PANDORA2025.git
make
ln -sf /np1a/v05/pandora/2025aug/d202510a/sakra/ridf_file ridf
mkdir root
./deco.mpv ridf/run0120.ridf root/run0120.root
root root/run0120.root
```
