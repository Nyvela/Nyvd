# Nyvd
A simple init system designed for Nyvela kernel.

Nyvd is built by Nyvela's makefile, not by this one directly. Nyvela globs
`src/` and `include/` as prerequisites of `$(INIT_BIN)` and runs this project's
own `make`, so a normal `make -C ../Nyvela` picks up changes here.

Two consequences worth knowing:

- Do not add a new source root without adding it to `INIT_INPUTS` in
  `../Nyvela/makefile`. Files outside `src/` and `include/` are invisible to that
  dependency graph, so edits to them get embedded stale.
- Nyvela embeds `build/nyvd.bin` with `incbin`. If you are unsure whether a
  change actually made it into the running image, check the binary rather than
  assuming:

  ```sh
  make && strings ../Nyvd/build/nyvd.bin | grep <path you just added>
  ```

  A missing string means a build problem, not a runtime one.

## Building standalone

```sh
USER_CFLAGS="-I../Nyvela/include" make
```

`USER_CFLAGS` is required: Nyvd includes `<nyvela/user/syslib.h>`, and without
that include path the build fails with
`fatal error: nyvela/user/syslib.h: No such file or directory`.
