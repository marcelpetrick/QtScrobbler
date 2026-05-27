# GitHub Actions — QtScrobbler

Two workflows automate building and publishing QtScrobbler.

---

## CI (`ci.yml`)

**Triggers:** every push to any branch (except version tags) and every pull request against `master`.

What it does:
1. Installs Qt 6, cmake, libmtp on an Ubuntu 24.04 runner
2. Runs `cmake -B build -S . -DCMAKE_BUILD_TYPE=Release`
3. Builds with `cmake --build build --parallel`
4. Smoke-tests `scrobbler --help` (exit 0) and checks `qtscrob` is executable
5. Uploads two artifacts:
   - **`build-report`** — markdown table of every stage result (kept 30 days)
   - **`binaries-linux-x86_64`** — `qtscrob` and `scrobbler` (kept 7 days)

Download artifacts from the **Actions** tab → select the workflow run → **Artifacts** section at the bottom of the run summary page.

---

## Release (`release.yml`)

**Trigger:** pushing a tag that matches `v[0-9]*` (e.g. `v0.13.5`).

### How to cut a release

```sh
# 1. Make sure the working tree is clean and on master
git checkout master
git pull

# 2. Create an annotated tag (use the version from src/lib/common.h)
git tag -a v0.13.5 -m "Release 0.13.5"

# 3. Push the tag — this triggers the release workflow
git push origin v0.13.5
```

The workflow then:
1. Installs Qt 6 and dependencies
2. Builds with CMake in Release mode
3. Smoke-tests the CLI binary
4. Strips debug symbols from both binaries (`strip --strip-unneeded`)
5. Packages them as `qtscrobbler-VERSION-linux-x86_64.tar.gz` together with `README.md`
6. Generates a build report with sizes and SHA-256 of the archive
7. Creates a GitHub Release with:
   - The tarball as a downloadable asset
   - The build report as the release body
8. Uploads the build report as an Actions artifact (kept 90 days)

The release appears at **Releases** on the repository's main page.

---

## Downloading and running a release

1. Go to the **Releases** page and pick the version you want.
2. Download `qtscrobbler-VERSION-linux-x86_64.tar.gz`.
3. Verify the SHA-256 listed in the release notes:

   ```sh
   sha256sum qtscrobbler-VERSION-linux-x86_64.tar.gz
   ```

4. Extract and run:

   ```sh
   tar -xzf qtscrobbler-VERSION-linux-x86_64.tar.gz
   cd qtscrobbler-VERSION-linux-x86_64

   ./qtscrob          # GUI
   ./scrobbler --help # CLI
   ```

### Runtime requirements

The binaries are dynamically linked against Qt 6 and libmtp. Install those on the target machine before running:

```sh
# Arch / Manjaro
sudo pacman -S qt6-base libmtp

# Debian / Ubuntu 24.04
sudo apt-get install qt6-base-dev libmtp9
```

---

## Notes

- The `GITHUB_TOKEN` is used automatically by the release workflow — no personal access token needed.
- CI and release both run on **Linux (Ubuntu 24.04) only**. macOS and Windows are not actively maintained.
- MTP device support (`libmtp`) is included in the release binaries when `libmtp-dev` is found at build time (it is on the GitHub-hosted runner).
