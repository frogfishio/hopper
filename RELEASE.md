# Release Checklist

Target: Hopper 1.0.0 (ABI version 1)

1) Bump versions (if needed)
   - `Makefile` pkg-config version
   - Docs mentioning current release (README, doc/abi.md, doc/getting_started.md)
   - CHANGELOG entry for new version
2) Verify build/tests
   - `make check` (build static/shared, run tests)
   - `make examples` (optional)
   - `make python-example` (optional)
   - `make catalog-load` (optional)
3) Packaging sanity
   - `make install DESTDIR=/tmp/hopper-dist` (check headers, libs, pkg-config)
4) Tag
   - `git tag -a v1.0.0 -m "Hopper 1.0.0"`
   - `git push origin v1.0.0`
5) Publish artifacts (optional)
   - Tarball of sources + built libs (if desired)
   - Update CI badges/status if applicable
6) Post-release
   - Update CHANGELOG to add "Unreleased" section for next work
