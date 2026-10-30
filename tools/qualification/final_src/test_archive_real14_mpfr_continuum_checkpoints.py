#!/usr/bin/env python3
"""Synthetic fail-closed checkpoint transport tests; NO release certificate."""
from __future__ import annotations
from hashlib import sha256
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch
from zipfile import ZipFile, ZIP_DEFLATED
import json
import unittest

from phase11_math_preflight import ContractError
from archive_real14_mpfr_continuum_checkpoints import (
    ARCHIVE_SCHEMA,package,verify,restore,_receipt)


class CheckpointTransportTests(unittest.TestCase):
    def setUp(self):
        self.temp=TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        self.original=self.root/'original-original-zip.bin'
        self.original.write_bytes(b'FAKE ORIGINAL TEST ONLY')
        self.checkpoints=self.root/'checkpoints'
        self.checkpoints.mkdir()
        self.archive=self.root/'receipts.zip'
        self.sha='b'*64
        self.engine='c'*64
        self.fake_authority=patch(
            'archive_real14_mpfr_continuum_checkpoints._authority',
            return_value=('a'*64,{(2,1):self.sha},self.engine))
        self.fake_authority.start()
        self.addCleanup(self.fake_authority.stop)
        self.name='L2_M1_passband_r.json'
        self.record={
            'schema':'rgsml.phase11.resumable-real14-dtft-continuum-nonrelease/1',
            'fin':44100,'fout':88200,'L':2,'M':1,'kind':'passband',
            'lo_hz':'0','hi_hz':'21168','bits':256,
            'candidate_sha256':self.sha,'engine_sha256':self.engine,
            'status':'PROVEN',
            'proof_result':{'status':'CONTINUUM_SUBBAND_PROVEN_NONRELEASE',
                            'visited_boxes':1,'proven_boxes':1,
                            'remaining_boxes':0,'release_qualified':False},
            'release_qualified':False,'product_admission_authorized':False}
        (self.checkpoints/self.name).write_text(json.dumps(self.record))

    def _pack(self):
        return package(self.checkpoints,self.original,self.archive)

    def test_roundtrip_atomic_and_exact_json(self):
        m=self._pack()
        self.assertEqual(m['count'],1)
        self.assertFalse(m['full_band_proof_certified'])
        self.assertEqual(m['schema'],ARCHIVE_SCHEMA)
        checked,files=verify(self.archive,self.original)
        self.assertEqual(checked,m)
        self.assertNotIn(b'FAKE ORIGINAL TEST ONLY',self.archive.read_bytes())
        dst=self.root/'new-receipts'
        restored=restore(self.archive,self.original,dst)
        self.assertEqual(restored,m)
        self.assertEqual((dst/self.name).read_bytes(),(self.checkpoints/self.name).read_bytes())
        with self.assertRaises(ContractError):restore(self.archive,self.original,dst)
        with self.assertRaises(ContractError):self._pack()

    def test_wrong_interval_or_engine_is_refused(self):
        for field,value in [('hi_hz','23041'),('candidate_sha256','d'*64),
                            ('engine_sha256','e'*64),('release_qualified',True)]:
            with self.subTest(field=field):
                data=dict(self.record)
                data[field]=value
                (self.checkpoints/self.name).write_text(json.dumps(data))
                with self.assertRaises(ContractError):self._pack()
        (self.checkpoints/self.name).write_text(json.dumps(self.record))

    def test_modified_truncated_extra_or_duplicate_archive_rejected(self):
        self._pack()
        with ZipFile(self.archive) as z:
            original=[(i,z.read(i.filename)) for i in z.infolist()]
        for kind in ('altered','missing','extra','duplicate','release'):
            with self.subTest(kind=kind):
                entries=list(original)
                if kind=='altered':
                    entries=[(i,b+b'X' if i.filename.endswith('.json') and i.filename.startswith('receipts/') else b) for i,b in entries]
                elif kind=='missing':
                    entries=[(i,b) for i,b in entries if i.filename=='MANIFEST_NONRELEASE.json']
                elif kind=='extra':
                    entries.append(('private/Q0.bin',b'SECRET'))
                elif kind=='duplicate':
                    entries.append(entries[-1])
                elif kind=='release':
                    entries=[(i,json.dumps(dict(json.loads(b),release_qualified=True)).encode()
                              if i.filename=='MANIFEST_NONRELEASE.json' else b)
                              for i,b in entries]
                bad=self.root/(kind+'.zip')
                with ZipFile(bad,'w',ZIP_DEFLATED) as z:
                    for i,b in entries:
                        z.writestr(i,b)
                with self.assertRaises(ContractError):verify(bad,self.original)
                self.assertFalse((self.root/(kind+'-restored')).exists())

    def test_unexpected_checkpoint_name_and_symlink_rejected(self):
        self._pack()
        self.assertEqual(len(verify(self.archive,self.original)[1]),1)
        (self.checkpoints/'private.bin').write_bytes(b'KERNEL-LEAK')
        with self.assertRaises(ContractError):package(self.checkpoints,self.original,self.root/'new.zip')
        (self.checkpoints/'private.bin').unlink()
        link=self.checkpoints/'L2_M1_stopband_r.json'
        link.symlink_to(self.checkpoints/self.name)
        with self.assertRaises(ContractError):package(self.checkpoints,self.original,self.root/'new.zip')

    def test_original_archive_checksum_authority_is_mandatory(self):
        self.fake_authority.stop()
        with self.assertRaises(ContractError):self._pack()
        self.fake_authority.start()


if __name__=='__main__':
    unittest.main(verbosity=2)