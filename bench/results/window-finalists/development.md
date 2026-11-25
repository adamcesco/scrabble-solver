# Paired development solve times in microseconds

## portable

| Left | Right | Early L/R | Middle L/R | Late L/R | Equal-stage L/R | Saving (95% CI) | Outcome |
| --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| packed | rack | 354.20 / 290.04 | 516.79 / 401.68 | 368.40 / 302.16 | 413.13 / 331.30 | +81.83 (+66.81, +96.64) | right faster |
| packed | neighbor-cache | 355.76 / 344.34 | 525.10 / 502.69 | 374.52 / 361.98 | 418.46 / 403.00 | +15.45 (+10.68, +20.22) | right faster |
| packed | rack-cache | 353.43 / 289.26 | 519.46 / 405.04 | 366.43 / 304.90 | 413.10 / 333.06 | +80.04 (+64.60, +95.57) | right faster |
| packed | rack-neighbor | 432.60 / 354.78 | 629.89 / 506.40 | 454.52 / 366.12 | 505.67 / 409.10 | +96.57 (+78.07, +115.51) | right faster |
| packed | rack-neighbor-cache | 364.31 / 292.90 | 545.79 / 417.56 | 384.06 / 315.38 | 431.39 / 341.95 | +89.44 (+67.33, +111.00) | right faster |
| packed | rack-all-checks | 358.78 / 287.03 | 510.80 / 419.90 | 364.90 / 310.25 | 411.49 / 339.06 | +72.43 (+42.18, +96.84) | right faster |
| rack | neighbor-cache | 291.14 / 364.53 | 422.57 / 530.31 | 301.36 / 355.47 | 338.36 / 416.77 | -78.41 (-108.78, -52.59) | right slower |
| rack | rack-cache | 292.17 / 287.45 | 397.53 / 392.49 | 294.94 / 294.13 | 328.21 / 324.69 | +3.52 (-0.07, +7.70) | tie |
| rack | rack-neighbor | 288.02 / 283.44 | 399.58 / 391.55 | 301.89 / 297.06 | 329.83 / 324.02 | +5.81 (+2.97, +8.56) | right faster |
| rack | rack-neighbor-cache | 295.56 / 286.59 | 403.89 / 397.10 | 311.05 / 301.34 | 336.84 / 328.35 | +8.49 (+5.17, +11.77) | right faster |
| rack | rack-all-checks | 300.95 / 289.54 | 411.31 / 401.13 | 302.33 / 298.06 | 338.20 / 329.58 | +8.62 (+5.09, +12.51) | right faster |
| neighbor-cache | rack-cache | 359.17 / 309.18 | 537.22 / 442.92 | 386.25 / 339.14 | 427.54 / 363.75 | +63.80 (+50.66, +77.51) | right faster |
| neighbor-cache | rack-neighbor | 343.97 / 284.26 | 495.85 / 396.08 | 368.04 / 310.32 | 402.62 / 330.22 | +72.40 (+58.94, +86.07) | right faster |
| neighbor-cache | rack-neighbor-cache | 348.92 / 287.11 | 498.57 / 404.00 | 358.24 / 298.52 | 401.91 / 329.88 | +72.03 (+58.38, +85.63) | right faster |
| neighbor-cache | rack-all-checks | 341.13 / 284.17 | 515.88 / 414.38 | 356.62 / 300.16 | 404.54 / 332.90 | +71.64 (+58.95, +84.12) | right faster |
| rack-cache | rack-neighbor | 294.73 / 291.34 | 415.93 / 409.68 | 313.49 / 305.93 | 341.39 / 335.65 | +5.74 (+2.22, +9.31) | right faster |
| rack-cache | rack-neighbor-cache | 290.52 / 284.33 | 402.28 / 397.09 | 290.95 / 287.16 | 327.91 / 322.86 | +5.06 (+1.51, +8.47) | right faster |
| rack-cache | rack-all-checks | 284.35 / 280.30 | 393.15 / 386.82 | 296.57 / 299.13 | 324.69 / 322.09 | +2.61 (-1.75, +6.58) | tie |
| rack-neighbor | rack-neighbor-cache | 283.28 / 280.81 | 398.84 / 407.60 | 295.51 / 295.35 | 325.88 / 327.92 | -2.04 (-6.29, +1.56) | tie |
| rack-neighbor | rack-all-checks | 287.66 / 286.16 | 403.10 / 397.67 | 295.42 / 298.25 | 328.73 / 327.36 | +1.37 (-1.62, +4.37) | tie |
| rack-neighbor-cache | rack-all-checks | 321.05 / 290.97 | 399.69 / 434.97 | 299.21 / 299.89 | 339.99 / 341.94 | -1.96 (-32.60, +28.95) | tie |
| row-cache | neighbor-cache | 347.96 / 342.77 | 506.25 / 499.70 | 364.08 / 356.74 | 406.10 / 399.74 | +6.36 (+3.39, +9.37) | right faster |
| neighbor-bits | neighbor-cache | 338.21 / 336.40 | 490.22 / 490.92 | 353.76 / 352.61 | 394.06 / 393.31 | +0.76 (-3.40, +5.43) | tie |
| row-cache | rack-cache | 338.00 / 286.75 | 507.61 / 403.82 | 359.08 / 301.03 | 401.56 / 330.53 | +71.03 (+57.69, +84.14) | right faster |
| neighbor-bits | rack-neighbor | 337.19 / 283.76 | 496.42 / 396.43 | 351.72 / 292.48 | 395.11 / 324.22 | +70.89 (+58.05, +83.55) | right faster |
| all-checks | rack-all-checks | 342.97 / 283.70 | 509.18 / 404.19 | 364.68 / 298.82 | 405.61 / 328.91 | +76.71 (+62.48, +90.77) | right faster |

Round-robin means (µs/solve), fastest first. rack-neighbor-cache 331.82, rack-all-checks 332.15, rack 333.79, rack-cache 335.92, rack-neighbor 342.26, neighbor-cache 409.40, packed 432.21.

Advance rack-neighbor-cache, rack-all-checks, rack-neighbor. Include packed as the matched baseline.

## skylake

| Left | Right | Early L/R | Middle L/R | Late L/R | Equal-stage L/R | Saving (95% CI) | Outcome |
| --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| packed | rack | 357.18 / 281.56 | 518.93 / 403.92 | 366.34 / 298.24 | 414.15 / 327.90 | +86.24 (+71.75, +100.17) | right faster |
| packed | neighbor-cache | 355.98 / 338.62 | 520.06 / 489.95 | 369.05 / 351.56 | 415.03 / 393.38 | +21.66 (+17.45, +25.83) | right faster |
| packed | rack-cache | 345.51 / 282.26 | 509.76 / 399.52 | 368.56 / 292.50 | 407.94 / 324.76 | +83.18 (+68.76, +97.54) | right faster |
| packed | rack-neighbor | 342.66 / 260.87 | 501.93 / 372.43 | 354.64 / 284.99 | 399.74 / 306.10 | +93.65 (+78.35, +109.07) | right faster |
| packed | rack-neighbor-cache | 347.38 / 279.68 | 514.98 / 386.58 | 366.02 / 291.78 | 409.46 / 319.35 | +90.11 (+74.50, +105.65) | right faster |
| packed | rack-all-checks | 359.86 / 293.91 | 521.87 / 408.41 | 379.77 / 312.89 | 420.50 / 338.40 | +82.10 (+66.59, +97.62) | right faster |
| rack | neighbor-cache | 285.01 / 341.88 | 415.19 / 507.27 | 300.48 / 353.62 | 333.56 / 400.92 | -67.36 (-78.32, -56.16) | right slower |
| rack | rack-cache | 274.61 / 288.06 | 398.35 / 409.58 | 294.96 / 301.44 | 322.64 / 333.02 | -10.38 (-14.79, -6.62) | right slower |
| rack | rack-neighbor | 282.59 / 274.02 | 401.91 / 397.54 | 298.04 / 294.46 | 327.51 / 322.01 | +5.50 (+2.17, +9.01) | right faster |
| rack | rack-neighbor-cache | 287.40 / 289.53 | 401.73 / 405.87 | 303.10 / 294.71 | 330.74 / 330.04 | +0.70 (-2.63, +3.88) | tie |
| rack | rack-all-checks | 294.98 / 302.77 | 432.37 / 442.61 | 317.83 / 331.44 | 348.40 / 358.94 | -10.55 (-16.78, -4.95) | right slower |
| neighbor-cache | rack-cache | 339.41 / 302.02 | 500.55 / 409.78 | 358.86 / 310.32 | 399.61 / 340.71 | +58.90 (+44.43, +73.10) | right faster |
| neighbor-cache | rack-neighbor | 335.63 / 272.56 | 500.96 / 397.40 | 352.76 / 289.76 | 396.45 / 319.91 | +76.55 (+63.35, +89.68) | right faster |
| neighbor-cache | rack-neighbor-cache | 328.11 / 276.99 | 484.75 / 389.44 | 345.27 / 288.93 | 386.04 / 318.45 | +67.59 (+54.81, +80.31) | right faster |
| neighbor-cache | rack-all-checks | 329.20 / 282.18 | 488.43 / 401.41 | 343.57 / 300.85 | 387.07 / 328.14 | +58.92 (+45.63, +72.00) | right faster |
| rack-cache | rack-neighbor | 293.02 / 272.18 | 402.43 / 384.87 | 300.49 / 299.09 | 331.98 / 318.72 | +13.26 (+10.21, +16.31) | right faster |
| rack-cache | rack-neighbor-cache | 290.33 / 281.21 | 414.66 / 397.45 | 300.17 / 291.05 | 335.05 / 323.24 | +11.82 (+8.61, +15.10) | right faster |
| rack-cache | rack-all-checks | 288.58 / 288.68 | 406.04 / 406.78 | 304.10 / 305.73 | 332.91 / 333.73 | -0.82 (-4.91, +3.30) | tie |
| rack-neighbor | rack-neighbor-cache | 268.16 / 280.44 | 394.07 / 397.57 | 291.46 / 297.79 | 317.90 / 325.26 | -7.37 (-11.22, -3.89) | right slower |
| rack-neighbor | rack-all-checks | 272.06 / 290.97 | 383.40 / 401.96 | 294.02 / 302.39 | 316.49 / 331.77 | -15.28 (-19.54, -11.10) | right slower |
| rack-neighbor-cache | rack-all-checks | 277.03 / 285.78 | 387.30 / 399.80 | 290.62 / 305.13 | 318.32 / 330.24 | -11.92 (-14.80, -9.06) | right slower |
| row-cache | neighbor-cache | 356.62 / 334.76 | 514.31 / 490.01 | 362.74 / 349.32 | 411.22 / 391.36 | +19.86 (+15.15, +24.84) | right faster |
| neighbor-bits | neighbor-cache | 317.41 / 335.07 | 475.11 / 488.49 | 347.03 / 358.97 | 379.85 / 394.17 | -14.32 (-19.13, -9.96) | right slower |
| row-cache | rack-cache | 358.65 / 294.35 | 524.33 / 409.25 | 368.24 / 300.07 | 417.07 / 334.56 | +82.51 (+67.79, +97.27) | right faster |
| neighbor-bits | rack-neighbor | 323.50 / 279.27 | 477.92 / 396.26 | 347.16 / 295.85 | 382.86 / 323.79 | +59.07 (+48.12, +69.96) | right faster |
| all-checks | rack-all-checks | 332.54 / 292.30 | 504.91 / 418.25 | 361.72 / 310.30 | 399.72 / 340.28 | +59.44 (+46.30, +72.73) | right faster |

Round-robin means (µs/solve), fastest first. rack-neighbor 316.85, rack-neighbor-cache 322.44, rack 331.79, rack-cache 333.07, rack-all-checks 336.87, neighbor-cache 393.91, packed 411.14.

Advance rack-neighbor, rack-neighbor-cache. Include packed as the matched baseline.
