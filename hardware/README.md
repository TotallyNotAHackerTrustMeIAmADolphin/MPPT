In V1.0 there are several issues.

- R1, R3 should be 5,1k but they are 5,1 ohm, which resultet in the inability to read voltage.
- C10, C11 where supposed to be low pass fitler for the current signal, but they just make it unreadable
- the 12V buck converter is dying
- XT60 PCB versions are switched polarity based on wheter they are male or female
- the Reset button needs a pinout
- the SWDIO/SWCLK pull downs are wrong

fixed so far in v1.1
- R1, R3 are now correct
