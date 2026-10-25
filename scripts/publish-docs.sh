#!/usr/bin/env bash
# Add one version of the API reference to a checkout of the gh-pages branch.
#
#   scripts/publish-docs.sh <html-dir> <site-dir> <version>
#
# Copies <html-dir> (Doxygen's output) to <site-dir>/<version>, replacing
# that version only, so earlier versions stay published. <version> is a tag
# (v1.0.0) or "main". Then rebuilds what links the versions together:
#   latest/       a copy of the newest release tag (not a pre-release such
#                 as v1.0.0-rc.1), or of main before the first one
#   index.html    redirects to latest/
#   versions.html lists every version, newest first
# Committing and pushing is the caller's job.

set -euo pipefail

if [ $# -ne 3 ]; then
    sed -n '2,13p' "$0" >&2
    exit 2
fi
html=$1
site=$2
version=$3
case "$version" in
    main | v[0-9]*) ;;
    *) echo "version must be main or a v* tag, not '$version'" >&2; exit 2 ;;
esac

rm -rf "${site:?}/$version"
cp -R "$html" "$site/$version"
touch "$site/.nojekyll"

tags=$(find "$site" -mindepth 1 -maxdepth 1 -type d -name 'v[0-9]*' -printf '%f\n' | sort -rV)
latest=$(printf '%s\n' "$tags" | grep -v -- - | head -n 1 || true)
latest=${latest:-main}
rm -rf "$site/latest"
cp -R "$site/$latest" "$site/latest"

cat > "$site/index.html" <<HTML
<!doctype html>
<meta charset="utf-8">
<title>SimpleOTA client API reference</title>
<meta http-equiv="refresh" content="0; url=latest/">
<link rel="canonical" href="latest/">
<p><a href="latest/">The latest API reference</a> (${latest}).
<a href="versions.html">All versions</a>.</p>
HTML

{
    cat <<HTML
<!doctype html>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>SimpleOTA client API reference: versions</title>
<style>body{font-family:system-ui,sans-serif;max-width:40rem;margin:2rem auto;padding:0 1rem;line-height:1.5}</style>
<h1>SimpleOTA client API reference</h1>
<p>Each core release has its own reference. A client release names the core
it carries in its <code>CORE_VERSION</code> file.</p>
<ul>
HTML
    for t in $tags; do
        note=""
        [ "$t" = "$latest" ] && note=" (latest)"
        echo "<li><a href=\"$t/\">$t</a>$note</li>"
    done
    if [ -d "$site/main" ]; then
        echo '<li><a href="main/">main</a> (unreleased)</li>'
    fi
    echo '</ul>'
} > "$site/versions.html"

echo "published $version; latest is $latest"
