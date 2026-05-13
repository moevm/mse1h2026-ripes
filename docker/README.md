## Build docker image

To build latest version of Ripes from `master` branch issue the command

```bash
docker build --rm --tag ripes -f ripes.dockerfile .
```

Or to build from specific branch

```bash
docker build --rm --build-arg BRANCH=my_branch --tag ripes:latest -f ripes.dockerfile .
```

## Run docker container

Enable external connection to your X server (run for each session)
```bash
xhost local:root
```
**Note:** you may add this line to `~/.xsessionrc` file to avoid having to run it for each session

After that, issue the command

```bash
docker run --rm -it --net=host -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix ripes:latest
```

## Web (WebAssembly) build

Ripes can be cross-compiled to WebAssembly using Qt for WASM and served via
nginx. Everything is wired up in `docker-compose.yml` as the `ripes-web` service.

Build and start:

```bash
docker compose build ripes-web
docker compose up -d ripes-web
```

Then open http://localhost:8080 in a modern browser. The browser loads
`Ripes.html` + `Ripes.js` + `Ripes.wasm` and renders the whole UI on a `<canvas>`
via WebGL.

To rebuild from a different branch:

```bash
docker compose build --build-arg BRANCH=my_branch ripes-web
```

Notes on the WASM build:
- Compiled with `RIPES_WITH_QPROCESS=OFF` and `RIPES_BUILD_TESTS=OFF` —
  `QProcess` and `QTest` are not available in the browser sandbox.
- Uses Qt's `wasm_multithread` kit. Pthreads in WASM require `SharedArrayBuffer`,
  which the browser only enables on a cross-origin-isolated page, so the bundled
  nginx config sets `Cross-Origin-Opener-Policy: same-origin` and
  `Cross-Origin-Embedder-Policy: require-corp`.
- `QT_VERSION` and `EMSDK_VERSION` build-args must stay in sync (see the table
  at the top of `ripes_wasm.dockerfile`). A mismatch loads the page but throws
  an uncaught C++ exception inside Qt's event loop right after `_main()`.
- The page injects a small semi-transparent **RESTART** button in the top-right
  corner. Clicking it does a hard reload of the WASM app — useful when the
  simulator hangs or crashes.
