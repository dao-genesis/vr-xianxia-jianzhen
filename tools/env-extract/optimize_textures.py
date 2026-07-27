#!/usr/bin/env python3
# optimize_textures.py — make an extracted official-env glTF practical to load on Quest.
#
# The raw hsl_extract dump keeps every RENDTXTR as a full-resolution PNG (calming: 248 PNGs,
# ~490 MB, 137 of them >=1024 px). A Quest 3 browser stalls decoding that as a background base.
# This post-step caps texture resolution and re-encodes: opaque -> JPEG, alpha-bearing -> PNG,
# then rewrites the glTF image URIs. officialBase is a fixed backdrop, so a resolution cap and
# JPEG on opaque surfaces is visually cheap and cuts the payload by ~10-20x.
#
#   optimize_textures.py <in.gltf> <outDir> [--max 1024] [--quality 85]
#
# Emits <outDir>/<name>.gltf, <name>.bin (copied), textures/*.{jpg,png}. Assets stay private
# (repo .gitignore excludes daoshu/private/); never distribute Meta-proprietary output.
import sys, os, json, shutil, argparse
from PIL import Image


def has_alpha(im):
    if im.mode in ("RGBA", "LA") or (im.mode == "P" and "transparency" in im.info):
        a = im.convert("RGBA").getchannel("A")
        lo, _ = a.getextrema()
        return lo < 250  # genuinely transparent pixels present
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("gltf")
    ap.add_argument("outDir")
    ap.add_argument("--max", type=int, default=1024, help="max texture dimension (px)")
    ap.add_argument("--quality", type=int, default=85, help="JPEG quality for opaque textures")
    a = ap.parse_args()

    srcDir = os.path.dirname(os.path.abspath(a.gltf))
    name = os.path.splitext(os.path.basename(a.gltf))[0]
    outTex = os.path.join(a.outDir, "textures")
    os.makedirs(outTex, exist_ok=True)

    g = json.load(open(a.gltf, encoding="utf-8"))

    # copy the geometry buffer verbatim
    for b in g.get("buffers", []):
        uri = b.get("uri")
        if uri and not uri.startswith("data:"):
            shutil.copyfile(os.path.join(srcDir, uri), os.path.join(a.outDir, uri))

    before = after = 0
    for img in g.get("images", []):
        uri = img.get("uri")
        if not uri:
            continue
        src = os.path.join(srcDir, uri)
        before += os.path.getsize(src)
        im = Image.open(src)
        w, h = im.size
        k = max(w, h)
        if k > a.max:
            s = a.max / k
            im = im.resize((max(1, round(w * s)), max(1, round(h * s))), Image.LANCZOS)
        stem = os.path.splitext(os.path.basename(uri))[0]
        if has_alpha(im):
            outName = stem + ".png"
            im.convert("RGBA").save(os.path.join(outTex, outName), "PNG", optimize=True)
        else:
            outName = stem + ".jpg"
            im.convert("RGB").save(os.path.join(outTex, outName), "JPEG",
                                   quality=a.quality, optimize=True, progressive=True)
        img["uri"] = "textures/" + outName
        after += os.path.getsize(os.path.join(outTex, outName))

    json.dump(g, open(os.path.join(a.outDir, name + ".gltf"), "w", encoding="utf-8"),
              separators=(",", ":"))
    print("[optimize] textures %d  %.1f MB -> %.1f MB  (max=%d q=%d)"
          % (len(g.get("images", [])), before / 1e6, after / 1e6, a.max, a.quality))


if __name__ == "__main__":
    main()
