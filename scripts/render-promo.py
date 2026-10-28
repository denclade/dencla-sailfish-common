#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Dennis Burgermeister
# SPDX-License-Identifier: GPL-3.0-or-later

"""Render README thumbnails, a captioned GIF tour and a website MP4."""

import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, required=True, help="App-specific JSON manifest in the media directory")
    parser.add_argument("--ffmpeg", default="ffmpeg")
    parser.add_argument("--thumbnails-only", action="store_true")
    args = parser.parse_args()
    media = args.config.resolve().parent
    config = json.loads(args.config.read_text())
    scenes = config["scenes"]
    for filename in config["thumbnails"] + [scene["image"] for scene in scenes]:
        if not (media / filename).is_file():
            parser.error(f"Missing image: {media / filename}")
    if not scenes or any(scene["duration"] <= 0 for scene in scenes):
        parser.error("Scenes must have positive durations")

    def ffmpeg(*options):
        subprocess.run([args.ffmpeg, "-hide_banner", "-loglevel", "error", "-y", *options], check=True)

    thumbnails = media / "thumbs"
    thumbnails.mkdir(exist_ok=True)
    for filename in config["thumbnails"]:
        ffmpeg("-i", str(media / filename), "-vf", "scale=220:-1:flags=lanczos",
               "-frames:v", "1", str(thumbnails / Path(filename).name))
    if args.thumbnails_only:
        return

    with tempfile.TemporaryDirectory(prefix="sailfish-promo-") as directory:
        scratch = Path(directory)
        playlist = []
        for index, scene in enumerate(scenes):
            filename, caption, duration = scene["image"], scene["caption"], scene["duration"]
            frame = scratch / f"frame-{index}.png"
            text = scratch / f"caption-{index}.txt"
            text.write_text(caption)
            # Add captions outside the original screenshot, preserving the UI.
            ffmpeg("-i", str(media / filename), "-vf",
                   "pad=iw:ih+80:0:0:color=0x071522,"
                   f"drawtext=textfile='{text}':fontcolor=white:fontsize=24:"
                   "x=(w-tw)/2:y=h-50", "-frames:v", "1", str(frame))
            playlist.extend([f"file '{frame}'", f"duration {duration}"])
        playlist.append(f"file '{frame}'")
        manifest = scratch / "scenes.txt"
        manifest.write_text("\n".join(playlist) + "\n")
        source = ["-f", "concat", "-safe", "0", "-i", str(manifest), "-t", str(sum(scene["duration"] for scene in scenes))]
        ffmpeg(*source, "-vf", "fps=12,format=yuv420p", "-c:v", "libx264",
               "-crf", "20", "-movflags", "+faststart", "-an", str(media / "workflow.mp4"))
        ffmpeg(*source, "-filter_complex",
               "fps=5,scale=360:-1:flags=lanczos,split[a][b];"
               "[a]palettegen=stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=3",
               "-loop", "0", str(media / "workflow.gif"))


if __name__ == "__main__":
    main()
