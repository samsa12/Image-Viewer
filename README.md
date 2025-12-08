# pix

pure c image viewer for windows

no dependencies, just vibes

---

## the philosophy

do less. do it right.

no plugins. no config files. no "pro version". no telemetry. no cloud sync.
just an image viewer that opens instantly and gets out of your way.

---

## why this exists

- **instant** — cold start in milliseconds, not seconds
- **tiny** — single exe under 1mb, fits on a floppy (if you still have one)
- **zero dependencies** — no runtime, no framework, no dll hell
- **memory smart** — lazy buffers, disk reload for reset, respects your ram
- **actually good upscaling** — lanczos-3 with multithreading, same math as photoshop
- **16k support** — upscale up to 32K if you got the ram
- **resource controls** — you decide how much cpu/ram it uses
- **dark mode** — because its 2025 and we have standards
- **animated gifs** — plays em smooth with proper frame timing
- **full editing** — brightness contrast saturation rotate flip crop sharpen blur + undo

---

## vs other viewers

| | this | windows photos | irfanview | imageglass |
|---|---|---|---|---|
| philosophy | do one thing well | be an ecosystem | swiss army knife | modern .NET |
| size | <1mb | 200mb+ | 3mb | 15mb+ |
| startup | instant | 2-3 sec | fast | ~1 sec |
| dependencies | none | uwp runtime | none | .NET 6+ |
| ram usage | minimal | heavy | moderate | moderate |
| bloat | none | AI, timeline, "memories" | plugins, batch tools | extensions |
| lanczos upscale | yes (up to 32K) | no | plugin | no |
| resource controls | yes | no | no | no |
| dark mode | yes (default) | yes | sorta | yes |
| open source | yes | no | no | yes |

---

## why its better (honest take)

**windows photos** wants to be your photo manager, AI editor, and cloud backup. you just wanted to see a jpg.

**irfanview** is a legend but the UI is stuck in 1998. plugin system adds complexity. great if you need batch processing, overkill for viewing.

**imageglass** is pretty and modern but needs .NET runtime installed. heavier cold start. more abstraction layers between you and your pixels.

**this viewer** hits a different spot: modern features (lanczos, smart memory, dark mode) with old-school engineering (pure C, single exe, instant startup). no compromises on what matters.

---

## controls

| key | what it does |
|-----|--------------|
| `o` | open file |
| `←` `→` | prev / next |
| `f11` or `f` | fullscreen |
| `0` | fit to window |
| `1` | actual size |
| `+` `-` | zoom |
| `scroll` | zoom at cursor |
| `drag` | pan around |
| `s` | slideshow |
| `ctrl+s` | save (png/jpg/bmp) |
| `ctrl+z` | undo |
| `q` | lanczos 2x upscale |
| `z` | toggle zoom overlay |
| `?` or `f1` | keyboard help |
| `shift+p` | reset to original |
| `f2` | settings panel |
| `i` | info panel |
| `t` | dark / light theme |
| `r` `l` | rotate |
| `h` `v` | flip |
| `ctrl+c` | copy |
| `del` | trash it |
| `e` | show in explorer |
| `w` | set as wallpaper |
| `p` | print |
| `esc` | exit |

**in settings panel:**
| `m` | cycle max size (8K/16K/32K) |
| `t` | cycle cpu threads |
| `w` | toggle memory warnings |

---

## batch mode

upscale all images in a folder from command line:

```
pix.exe --batch-upscale C:\photos 2
```

- first arg: folder path
- second arg: scale factor (default 2x)
- outputs to `upscaled\` subfolder

---

## formats

png, jpg, bmp, gif (animated), tga, psd, hdr, pic, pnm

---

## build

```
build.bat
```

needs msvc or mingw

---

## license

free to use, modify, and share

but:
- **credit me** — keep my name somewhere, dont pretend you made it
- **no claiming ownership** — you can fork it, mod it, sell it even, just dont say its originally yours
- **no warranty** — this thing comes as is, if it breaks something thats on you
- **no liability** — if your files get deleted, corrupted, or your pc explodes, not my problem
- **no drm** — dont lock it down or add restrictions to the code

thats it. have fun with it
