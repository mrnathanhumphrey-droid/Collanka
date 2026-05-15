# macOS Release Pipeline — Hard-Won Notes

Short post-mortem of three traps the CI release flow fell into. Apply to any
JUCE plugin repo that ships a Mac VST3/AU/Standalone via GitHub Actions.

## Symptom

Downloaded `.app` (or `.vst3` / `.component`) refuses to launch on Apple
Silicon. Console / Terminal shows:

```
The application cannot be opened for an unexpected reason, error=
Error Domain=RBSRequestErrorDomain Code=5 "Launch failed."
NSUnderlyingError = NSPOSIXErrorDomain Code=111 "Launchd job spawn failed"
```

`xattr -cr` to strip quarantine and `codesign --force --deep --sign -` to
re-sign do not fix it. `codesign -dv --verbose=4` on the bundle reports
`Sealed Resources rules=13 files=1` — should be hundreds.

## Root cause: three layered bugs in the CI release path

### 1. `cp -R` strips macOS bundle integrity

The Package step copied bundles from the build artefacts dir to the dist
dir with `cp -R`. On macOS, `cp -R` does not preserve resource forks,
extended attributes, or code-signature integrity. The bundle keeps its
directory tree and `Info.plist` but loses the sealed-resource manifest
and the inner binary's launchd-relevant metadata.

**Fix:** use `ditto`, which is the macOS-native bundle-aware copy.

```bash
ditto "$src.app" "$dst.app"
```

Both in the CI package step and in `scripts/install_mac.sh`.

### 2. JUCE AU build hangs on headless CI runners

After fixing (1), the `Build` step hung indefinitely at ~90/92 ninja steps
on macos-14 runners (16+ minutes, vs. ~7 min for the build that succeeded).
The hang sits between the VST3 link and the AU bundle finalisation,
because JUCE invokes an `auval`-style validation that blocks waiting for
the audio-component registrar — which doesn't exist on a headless CI
runner.

**Fix:** make AU a CMake option, default ON for local Mac dev, force OFF
in CI.

```cmake
option(BUILD_AU "Build Audio Unit plugin format (macOS only)" ON)
set(PLUGIN_FORMATS VST3 Standalone)
if(BUILD_AU AND APPLE)
    list(APPEND PLUGIN_FORMATS AU)
endif()

juce_add_plugin(Collonka
    ...
    FORMATS ${PLUGIN_FORMATS}
    ...
)
```

CI configure step passes `-DBUILD_AU=OFF`. Logic Pro users build locally.

### 3. `actions/upload-artifact@v4` strips bundle integrity (the silent killer)

After fixing (1) and (2), CI completed cleanly in ~6 min and published a
release zip. Bundle inside the zip was still broken. The same launchd
RBS Code=5 error.

Reason: `actions/upload-artifact@v4` tars files internally on the runner,
uploads the tar, then `actions/download-artifact@v4` untars on the
release runner. Tar does not preserve macOS xattrs / resource forks by
default. The bundle survives `ditto` on the build runner, then dies in
the tar round trip, then the release job's `ditto -c -k` zips an
already-stripped bundle.

**Fix:** create the final user-facing `.zip` ON THE BUILD RUNNER, so the
artifact uploaded is an opaque zip blob (which tar treats as a single
file with no internal structure to lose). The release job attaches the
downloaded zips directly without re-extracting.

```yaml
# In Build job, after Package step:
- name: Zip for release (macOS)
  if: runner.os == 'macOS'
  run: ditto -c -k --sequesterRsrc --keepParent dist/$NAME $NAME.zip

- name: Upload artifact
  uses: actions/upload-artifact@v4
  with:
    name: $NAME
    path: $NAME.zip      # the file, not the dir

# In Release job:
- name: Create GitHub Release
  uses: softprops/action-gh-release@v2
  with:
    files: artifacts/**/*.zip   # already zipped, attach as-is
```

## End-to-end working path

```
build runner:
  cmake build
  juce ad-hoc signs the bundle
  Package step: ditto bundle → dist/
  Zip step:     ditto -c -k --sequesterRsrc --keepParent dist/$NAME $NAME.zip
  upload-artifact $NAME.zip            ← opaque blob, no bundle to strip

release runner:
  download-artifact (still opaque blob)
  softprops/action-gh-release attaches as-is

user:
  downloads .zip from Releases page
  Finder / Archive Utility expands → intact bundle
  install.sh uses ditto to copy into ~/Library/Audio/Plug-Ins/
  launchd spawns cleanly
```

## How to know it actually works

Codesign verify after download + unzip:

```bash
codesign -dv --verbose=4 "/path/to/Collonka.app"
```

Look for `Sealed Resources rules=13 files=N` where **N is in the hundreds**.
If N is small (1–3), the bundle is still broken somewhere in the pipeline.

## Cross-references

This repo is a fork of BillyWonkaVST. The same fixes landed there:

- BillyWonkaVST `d9d47bb` — cp -R → ditto
- BillyWonkaVST `4a785c7` — BUILD_AU=OFF on CI
- BillyWonkaVST `04ae3b7` — zip-on-runner workflow restructure

Reference those commits if anything regresses here or you spin up a third
plugin from the same template.
