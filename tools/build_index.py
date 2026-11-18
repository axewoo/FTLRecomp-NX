#!/usr/bin/env python3
"""Build index.json (a tico Store repository) from packages.json and the GitHub releases of each port.

Format: https://ticoverse.com/wiki/store/creating-a-store-repository (schema 1).
Each package takes the newest release of its repo that carries its asset (pre-releases only when the
package says "prerelease": true). Version, download URL, size, SHA-256 and changelog come from that release.
Set GITHUB_TOKEN to avoid the API's anonymous rate limit.
"""
import json
import os
import re
import sys
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ID_RE = re.compile(r"^[a-z0-9._-]{1,64}$")
CONFIG_ONLY = {"repo", "asset", "prerelease"}


def api(path):
    req = urllib.request.Request("https://api.github.com/" + path)
    req.add_header("Accept", "application/vnd.github+json")
    req.add_header("User-Agent", "tico-repo-index")
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        req.add_header("Authorization", "Bearer " + token)
    with urllib.request.urlopen(req, timeout=30) as resp:
        return json.load(resp)


def pick_release(repo, asset_name, allow_prerelease):
    for rel in api(f"repos/{repo}/releases?per_page=30"):
        if rel.get("draft") or (rel.get("prerelease") and not allow_prerelease):
            continue
        for asset in rel.get("assets", []):
            if asset["name"] == asset_name and asset.get("state", "uploaded") == "uploaded":
                return rel, asset
    return None, None


def changelog(body, limit=1500):
    text = (body or "").replace("\r\n", "\n").strip()
    # GitHub's generated notes link every PR; the Switch can't open links, so keep the text.
    text = re.sub(r" by @\S+ in https://github\.com/\S+", "", text)
    text = re.sub(r"\*\*Full Changelog\*\*: \S+", "", text).strip()
    return text if len(text) <= limit else text[: limit - 1].rstrip() + "…"


def main():
    with open(os.path.join(ROOT, "packages.json")) as f:
        config = json.load(f)
    index = dict(config["repository"])
    index["packages"] = []
    failed = []
    for spec in config["packages"]:
        if not ID_RE.match(spec["id"]):
            sys.exit(f"invalid id {spec['id']!r}")
        rel, asset = pick_release(spec["repo"], spec["asset"], spec.get("prerelease", False))
        if not asset:
            print(f"warning: no release of {spec['repo']} carries {spec['asset']}", file=sys.stderr)
            failed.append(spec["id"])
            continue
        pkg = {k: v for k, v in spec.items() if k not in CONFIG_ONLY}
        pkg["version"] = rel["tag_name"]
        pkg["type"] = "nro" if spec["asset"].endswith(".nro") else "zip"
        pkg["url"] = asset["browser_download_url"]
        pkg["size"] = asset["size"]
        digest = asset.get("digest") or ""
        if digest.startswith("sha256:"):
            pkg["sha256"] = digest.split(":", 1)[1]
        notes = changelog(rel.get("body"))
        if notes:
            pkg.setdefault("changelog", notes)
        index["packages"].append(pkg)
        print(f"{spec['id']}: {rel['tag_name']} {asset['size']} bytes", file=sys.stderr)
    if not index["packages"]:
        sys.exit("no package resolved; keeping the old index.json")
    out = os.path.join(ROOT, "index.json")
    with open(out, "w") as f:
        json.dump(index, f, indent=2, ensure_ascii=False)
        f.write("\n")
    if failed:
        print("missing: " + ", ".join(failed), file=sys.stderr)


if __name__ == "__main__":
    main()
