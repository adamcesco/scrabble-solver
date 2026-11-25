# Paired branch-alignment trials in microseconds

## skylake

| Left | Right | Early L/R | Middle L/R | Late L/R | Equal-stage L/R | Saving (95% CI) | Outcome | Outside pilot saving (95% CI) |
| --- | --- | ---: | ---: | ---: | ---: | ---: | --- | ---: |
| skylake.packed | skylake-unaligned.packed | 432.92 / 431.90 | 667.64 / 690.66 | 513.31 / 493.98 | 537.96 / 538.85 | -0.89 (-19.97, +18.83) | tie | -0.32 (-22.26, +21.42) |
| rack-neighbor | rack-neighbor-cache | 293.62 / 302.91 | 421.74 / 428.35 | 355.31 / 331.02 | 356.89 / 354.09 | +2.80 (-10.49, +24.80) | tie | +5.91 (-10.24, +32.87) |
| skylake.rack-neighbor | skylake-unaligned.rack-neighbor | 296.33 / 309.37 | 449.74 / 458.71 | 337.13 / 340.89 | 361.07 / 369.65 | -8.59 (-14.38, -3.13) | right slower | -8.90 (-15.88, -2.68) |
| skylake.rack-neighbor | skylake-unaligned.rack-neighbor-cache | 278.43 / 285.51 | 402.36 / 408.94 | 295.35 / 294.17 | 325.38 / 329.54 | -4.16 (-10.00, +0.74) | tie | -3.44 (-10.37, +2.58) |
| skylake.rack-neighbor-cache | skylake-unaligned.rack-neighbor | 280.31 / 284.62 | 396.24 / 395.12 | 294.13 / 296.97 | 323.56 / 325.57 | -2.01 (-4.78, +0.44) | tie | -0.53 (-2.48, +1.56) |
| skylake.rack-neighbor-cache | skylake-unaligned.rack-neighbor-cache | 277.91 / 277.55 | 388.81 / 388.21 | 287.39 / 286.27 | 318.04 / 317.34 | +0.70 (-1.99, +3.70) | tie | +1.05 (-2.19, +4.73) |

## skylake-unaligned

| Left | Right | Early L/R | Middle L/R | Late L/R | Equal-stage L/R | Saving (95% CI) | Outcome | Outside pilot saving (95% CI) |
| --- | --- | ---: | ---: | ---: | ---: | ---: | --- | ---: |
| rack-neighbor | rack-neighbor-cache | 286.40 / 312.26 | 420.13 / 405.22 | 302.95 / 294.49 | 336.49 / 337.32 | -0.83 (-22.39, +14.03) | tie | +11.30 (+5.07, +20.32) |
| packed | rack-neighbor | 382.43 / 304.49 | 555.86 / 444.44 | 393.99 / 322.86 | 444.09 / 357.26 | +86.83 (+69.86, +103.02) | right faster | +88.16 (+68.06, +107.53) |
| rack | rack-neighbor | 297.48 / 290.72 | 424.15 / 408.84 | 305.90 / 299.81 | 342.51 / 333.12 | +9.39 (+5.61, +13.61) | right faster | +9.45 (+5.43, +13.97) |
| packed | rack-neighbor-cache | 346.34 / 278.31 | 526.97 / 404.60 | 365.36 / 296.13 | 412.89 / 326.35 | +86.54 (+71.08, +101.45) | right faster | +90.70 (+72.52, +107.78) |
| rack | rack-neighbor-cache | 296.27 / 285.10 | 415.40 / 402.05 | 308.03 / 300.22 | 339.90 / 329.12 | +10.77 (+7.70, +13.82) | right faster | +10.70 (+7.25, +14.19) |
