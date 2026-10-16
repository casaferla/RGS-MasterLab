#!/usr/bin/env python3
"""A tiny, fully local WAV for SkinPack enabled-state appearance tests only.

Never a production source, no copyrighted audio, no playback required. This file
uses a deterministic non-silent PCM16 signal to exercise the actual source-open
and playback-ready bindings instead of overriding QML enabled properties.
"""
from pathlib import Path
import argparse
import math
import struct
import wave

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("output",type=Path)
    args=parser.parse_args()
    args.output.parent.mkdir(parents=True,exist_ok=True)
    rate=48000
    with wave.open(str(args.output),"wb") as out:
        out.setnchannels(2)
        out.setsampwidth(2)
        out.setframerate(rate)
        block=bytearray()
        for n in range(rate * 2):
            x=int(0.12*32767*math.sin(2*math.pi*440*n/rate))
            block.extend(struct.pack("<hh",x,x))
        out.writeframes(block)
    print("SKINPACK_ART_VISUAL_SOURCE_CREATED_PCM16_STEREO_48000HZ_TWO_SECONDS")

if __name__=="__main__":
    main()
