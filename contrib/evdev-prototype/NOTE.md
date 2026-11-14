# Note (added by the Mango packaging)

This directory is the standalone C++/libevdev prototype from the recovered ZIP, kept
unchanged and **not part of the Mango feature**. Its README says the C module was
"transcribed exactly"; that is not accurate for the C file that was in the ZIP (it was
a later rewrite). It also uses different detection constants than the Mango module
(140 px / 700 ms / 1199 ms retrigger guard versus 90 px / 500 ms / configurable cooldown).
The installer does not build or install it.
