#!/bin/bash
# Makes Release/Reference-Silk-<version>-Mac.pkg from the built VST3 and AU bundles. Runs on macOS (the GitHub workflow does this).
set -euo pipefail
cd "$(dirname "$0")/../.."
VERSION=$(sed -n 's/^project(ReferenceSilk VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
NAME="Reference Silk"
WORK=build/pkg
rm -rf "$WORK"; mkdir -p "$WORK/vst3/Library/Audio/Plug-Ins/VST3" "$WORK/au/Library/Audio/Plug-Ins/Components" "$WORK/res" "$WORK/scripts-au" "$WORK/scripts-vst3" Release
ART="build/ReferenceSilk_artefacts/Release"
[ -d "$ART" ] || ART="build/ReferenceSilk_artefacts"
cp -R "$ART/VST3/$NAME.vst3" "$WORK/vst3/Library/Audio/Plug-Ins/VST3/"
cp -R "$ART/AU/$NAME.component" "$WORK/au/Library/Audio/Plug-Ins/Components/"
# Ad-hoc signature: needed for the plugins to load on Apple Silicon.
codesign --force --deep --sign - "$WORK/vst3/Library/Audio/Plug-Ins/VST3/$NAME.vst3"
codesign --force --deep --sign - "$WORK/au/Library/Audio/Plug-Ins/Components/$NAME.component"

# remove an older copy first, and make Logic see the new Audio Unit right away
cat > "$WORK/scripts-vst3/preinstall" <<'S'
#!/bin/bash
rm -rf "/Library/Audio/Plug-Ins/VST3/Reference Silk.vst3"
exit 0
S
cat > "$WORK/scripts-au/preinstall" <<'S'
#!/bin/bash
rm -rf "/Library/Audio/Plug-Ins/Components/Reference Silk.component"
exit 0
S
cat > "$WORK/scripts-au/postinstall" <<'S'
#!/bin/bash
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
exit 0
S
chmod +x "$WORK"/scripts-*/*

# The VST3 and the AU share a bundle ID, so the installer must never "relocate" one onto the other.
for k in vst3 au; do
    pkgbuild --analyze --root "$WORK/$k" "$WORK/$k.plist"
    i=0
    while /usr/libexec/PlistBuddy -c "Print :$i" "$WORK/$k.plist" >/dev/null 2>&1; do
        /usr/libexec/PlistBuddy -c "Delete :$i:BundleIsRelocatable" "$WORK/$k.plist" >/dev/null 2>&1 || true
        /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$WORK/$k.plist"
        i=$((i + 1))
    done
done
pkgbuild --root "$WORK/vst3" --component-plist "$WORK/vst3.plist" --identifier com.reference.silk.vst3 --version "$VERSION" --install-location / --scripts "$WORK/scripts-vst3" "$WORK/vst3.pkg"
pkgbuild --root "$WORK/au" --component-plist "$WORK/au.plist" --identifier com.reference.silk.au --version "$VERSION" --install-location / --scripts "$WORK/scripts-au" "$WORK/au.pkg"

cp Installer/mac/welcome.html "$WORK/res/"
productbuild --distribution Installer/mac/distribution.xml --resources "$WORK/res" --package-path "$WORK" "Release/Reference-Silk-$VERSION-Mac.pkg"
echo "Made Release/Reference-Silk-$VERSION-Mac.pkg"
