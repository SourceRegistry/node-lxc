## [1.1.1](https://github.com/SourceRegistry/node-lxc/compare/v1.1.0...v1.1.1) (2026-05-26)


### Bug Fixes

* use unique asset names for GitHub release to avoid name collision ([8cdf0a7](https://github.com/SourceRegistry/node-lxc/commit/8cdf0a73c823e008d6a325fdc6d246d8189fae09))

# [1.1.0](https://github.com/SourceRegistry/node-lxc/compare/v1.0.1...v1.1.0) (2026-05-26)


### Bug Fixes

* use -llxc for arch-neutral linking and set gypfile false to prevent npm rebuild on install ([f4d4350](https://github.com/SourceRegistry/node-lxc/commit/f4d4350f87ea6d7325d66f6defeb1613d7333070))


### Features

* add arm64 support and execOutput/execAsync methods ([3205c46](https://github.com/SourceRegistry/node-lxc/commit/3205c46e519b4b6d57ffb30d6e831c75a8d403e6))

## [1.0.1](https://github.com/SourceRegistry/node-lxc/compare/v1.0.0...v1.0.1) (2026-05-25)


### Bug Fixes

* **cmake:** include all headers and fix include paths for CLion ([78a4d9f](https://github.com/SourceRegistry/node-lxc/commit/78a4d9f433c0933b2bb6e58a2706bf60c88655d2))
* **cmake:** quote NODE_ADDON_API_DIR in string(REGEX REPLACE) to avoid empty-variable error ([f458203](https://github.com/SourceRegistry/node-lxc/commit/f4582034326c2a4cac59bc828f82367938d410c2))
* **cmake:** resolve napi.h and uv.h without depending on node being on PATH ([e036113](https://github.com/SourceRegistry/node-lxc/commit/e03611340e203dcfef8a03fb3cc1442edbbbd588))
* **docs:** trigger for release ([ee1d3ca](https://github.com/SourceRegistry/node-lxc/commit/ee1d3ca925fc0e640c11e3a1ce37c370ed3d7187))
* remove gypfile to stop npm install auto-triggering node-gyp rebuild ([7e31eb6](https://github.com/SourceRegistry/node-lxc/commit/7e31eb6f0ecfdf5bb383174b931127b3337116a5))

# 1.0.0 (2026-05-25)


### Bug Fixes

* better close handling on console ([ecbf451](https://github.com/SourceRegistry/node-lxc/commit/ecbf451fafaf78af8014eccdec0ee42559761d74))
* **ci:** use npx node-gyp and npx tsc in CI and build scripts ([5c41389](https://github.com/SourceRegistry/node-lxc/commit/5c4138935c623f93c585b7449894c8cc7f280382))
* lib path in binding.gyp ([0052309](https://github.com/SourceRegistry/node-lxc/commit/00523090a749d7ee898dc736685e686a26694c33))
* ready to merge into main ([9f711ae](https://github.com/SourceRegistry/node-lxc/commit/9f711aec766898cf5a39a0b4cc6ac9d726c4e88b))
* remove log lines ([976dfed](https://github.com/SourceRegistry/node-lxc/commit/976dfed8ef642d61ec5c9bfa0dc6561c858fa2cc))
* segfault on consoleAsync and added onclose ([7affdab](https://github.com/SourceRegistry/node-lxc/commit/7affdabc2d384e4686ff1e5a31895e99f9f7fa45))


### Features

* production-ready 1.0.0 release ([cec22a8](https://github.com/SourceRegistry/node-lxc/commit/cec22a8610143d0d1586adc266a9fab54d8c4093))
