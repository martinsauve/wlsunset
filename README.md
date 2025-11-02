# TODO: check if the timezone parsing works on BSD, NIX and other distros than Arch/Manjaro. Wayland compositors don't matter for this.

# wlsunset

This fork implements basic autodetection of the location based on the timezone of the machine using TZDB. Should work everywhere but only tested on Manjaro and Arch, help welcome!

The original repo on github is just a mirror, I would need to submit the patch via a mailing list if i wanted to upstream...

Day/night gamma adjustments for Wayland compositors supporting `wlr-gamma-control-unstable-v1`.

# How to build and install

```
meson build
ninja -C build
sudo ninja -C build install
```

# How to use

See the helptext (`wlsunset -h`)

## Example

```
# Beijing lat/long.
wlsunset -l 39.9 -L 116.3 -t 5000 -T 6500
```
Greater precision than one decimal place [serves no purpose](https://xkcd.com/2170/) other than padding the command-line.

## Location autodetect

If no lat/lon is provided, tries to read it from the `TZ` environment variable or the local time set on the machine.

```
TZ='Europe/Paris' wlsunset
>> inferred location from timezone Europe/Paris: lat 48.86667, lon 2.333333
>> ...
```

```
wlsunset
>> inferred location from timezone America/Los_Angeles: lat 34.052222, long -118.242778
>> ...
```

# Help

Go to #kennylevinsen @ irc.libera.chat to discuss, or use [~kennylevinsen/wlsunset-devel@lists.sr.ht](https://lists.sr.ht/~kennylevinsen/wlsunset-devel)
