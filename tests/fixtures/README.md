# tagforge test fixtures

## `java/bigtest_*.nbt`

Vendored from [Celisium/libnbt](https://github.com/Celisium/libnbt) under its
**CC0 1.0 Universal** license. The three files contain the same NBT tree
(Notch's classic "bigtest") encoded as raw, gzip, and zlib respectively. We
ship them verbatim so byte-exact round-trip tests have a known-good corpus.

Source: <https://github.com/Celisium/libnbt/blob/main/bigtest_raw.nbt> (and the
sibling `.nbt` files in the same directory).

## Hand-rolled fixtures

Small fixtures synthesised by the tagforge test setup to exercise edge cases
the bigtest corpus doesn't cover. Each is a few bytes long; the source bytes
are documented inline in the test that consumes them.

## Synthesised fixtures

`region/r.0.0.mca` is built at test-setup time by encoding a known tree with
tagforge's own writer and packing it with the region writer. Avoids vendoring
an arbitrary Minecraft world.
