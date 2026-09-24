# xBRZ

Unmodified xBRZ source by Zenju, copied from the `xbrz/` directory of
https://github.com/atheros/xbrzscale at revision
`a2d8dce723e8fab548bf8460b46eeebfc64abca6`.

This is the same implementation used in the offline character comparison.
See `License.txt` and the source headers for the upstream GPL license and
exceptions. The source is included locally so builds do not download it.

`Client/SpriteLib/SpriteGpuXbrz.h` is this project's GPU adaptation of the RGB
2x/3x/4x scaler. It retains attribution and the GPLv3 terms; upstream source
here remains unmodified. The GPU adaptation does not extend upstream's linking
exceptions to the new file. Renderer tests use this CPU implementation as the
output oracle for the shader path.
