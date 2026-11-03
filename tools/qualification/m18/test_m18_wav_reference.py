#!/usr/bin/env python3
"""Independent standard-library WAV/RF64 structure and quantization fixtures.
This does not consume or import the C++ implementation. Tests establish
container bytes for future binary equivalence; not a final-export FQ.
"""
from __future__ import annotations

import io
import struct
import unittest
import wave

from test_m18_independent_reference import independent_quantize

SEED = 0x0123456789ABCDEF


def pcm24_signed_little_endian(n: int) -> bytes:
    if not -8388608 <= n <= 8388607:
        raise OverflowError("PCM24 integer out of bounds")
    return (n & 0xFFFFFF).to_bytes(3, "little")


def canonical_pcm24_header(sample_rate: int, channels: int,
                           frame_count: int) -> bytes:
    data_len = frame_count * channels * 3
    assert data_len + 36 <= 0xFFFFFFFE
    return (b"RIFF" + struct.pack("<I", data_len + 36) + b"WAVE"
            + b"fmt " + struct.pack("<IHHIIHH", 16, 1, channels,
                                     sample_rate, sample_rate*channels*3,
                                     channels*3, 24)
            + b"data" + struct.pack("<I", data_len))


def canonical_float32_header(sample_rate: int, channels: int,
                             frame_count: int) -> bytes:
    data_len = frame_count * channels * 4
    assert data_len + 50 <= 0xFFFFFFFE
    return (b"RIFF" + struct.pack("<I", data_len + 50) + b"WAVE"
            + b"fmt " + struct.pack("<IHHIIHHH", 18, 3, channels,
                                     sample_rate, sample_rate*channels*4,
                                     channels*4, 32, 0)
            + b"fact" + struct.pack("<II", 4, frame_count)
            + b"data" + struct.pack("<I", data_len))


class WavReferenceTests(unittest.TestCase):
    def test_pcm24_empty_mono(self) -> None:
        self.assertEqual(
            canonical_pcm24_header(44100, 1, 0).hex(),
            ("524946462400000057415645666d742010000000"
             "0100010044ac0000cc040200030018006461746100000000"))
        self.assertEqual(len(canonical_pcm24_header(44100,1,0)), 44)

    def test_pcm24_stereo_with_independent_dither_bytes(self) -> None:
        samples = (0.0,0.5,-0.5,0.25)
        codes = [independent_quantize(x,SEED,11 + i//2,i%2)
                 for i,x in enumerate(samples)]
        body = b"".join(pcm24_signed_little_endian(q) for q in codes)
        self.assertEqual(len(body),12)
        blob = canonical_pcm24_header(48000,2,2)+body
        self.assertEqual(len(blob),56)
        self.assertEqual(struct.unpack_from("<I",blob,4)[0],48)
        self.assertEqual(struct.unpack_from("<I",blob,28)[0],288000)
        with wave.open(io.BytesIO(blob),"rb") as audio:
            self.assertEqual(audio.getnchannels(),2)
            self.assertEqual(audio.getsampwidth(),3)
            self.assertEqual(audio.getframerate(),48000)
            self.assertEqual(audio.getnframes(),2)
            self.assertEqual(audio.readframes(2),body)

    def test_float32_fact_without_seed(self) -> None:
        samples=(0.0,0.5,-0.5,0.25)
        header=canonical_float32_header(48000,2,2)
        body=b"".join(struct.pack("<f",x) for x in samples)
        self.assertEqual(len(header),58)
        self.assertEqual(len(header+body),74)
        self.assertEqual(struct.unpack_from("<H",header,20)[0],3)
        self.assertEqual(header[38:42],b"fact")
        self.assertEqual(struct.unpack_from("<I",header,46)[0],2)
        self.assertEqual(struct.unpack_from("<I",header,54)[0],16)
        # IEEE_FLOAT32 has no RNG seed; any supplied seed is irrelevant.
        self.assertEqual(body,b"\x00\x00\x00\x00\x00\x00\x00?"
                              b"\x00\x00\x00\xbf\x00\x00\x80>")

    def test_rf64_ds64_authority_geometry(self) -> None:
        frame_count = 715827883
        data_len=frame_count*6
        file_len=data_len+80
        self.assertEqual(data_len,4294967298)
        header=(b"RF64"+struct.pack("<I",0xFFFFFFFF)+b"WAVE"
                +b"ds64"+struct.pack("<IQQQI",28,file_len-8,data_len,
                                      frame_count,0)
                +b"fmt "+struct.pack("<IHHIIHH",16,1,2,192000,
                                      192000*6,6,24)
                +b"data"+struct.pack("<I",0xFFFFFFFF))
        self.assertEqual(len(header),80)
        self.assertEqual(header[12:16],b"ds64")
        self.assertEqual(struct.unpack_from("<Q",header,20)[0],file_len-8)
        self.assertEqual(struct.unpack_from("<Q",header,28)[0],data_len)
        self.assertEqual(struct.unpack_from("<Q",header,36)[0],frame_count)


if __name__=="__main__":
    unittest.main(verbosity=2)
