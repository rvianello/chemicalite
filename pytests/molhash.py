import unittest
import sys

from chemicalite import ChemicaLiteTestCase

class TestMolHash(ChemicaLiteTestCase):

    def test_molhash(self):
        c = self.db.cursor()

        smiles = 'C1CCCC(O)C1c1ccnc(OC)c1'

        tests = [
            ('mol_hash_anonymousgraph', '***1****(*2*****2*)*1'),
            ('mol_hash_elementgraph', 'COC1CC(C2CCCCC2O)CCN1'),
            ('mol_hash_canonicalsmiles', 'COc1cc(C2CCCCC2O)ccn1'),
            ('mol_hash_murckoscaffold', 'c1cc(C2CCCCC2)ccn1'),
            ('mol_hash_extendedmurcko', '*c1cc(C2CCCCC2*)ccn1'),
            ('mol_hash_molformula', 'C12H17NO2'),
            ('mol_hash_atombondcounts', '15,16'),
            ('mol_hash_degreevector', '0,4,9,2'),
            ('mol_hash_mesomer', 'CO[C]1[CH][C](C2CCCCC2O)[CH][CH][N]1_0'),
            ('mol_hash_regioisomer', '*O.*O*.C.C1CCCCC1.c1ccncc1'),
            ('mol_hash_netcharge', '0'),
            ('mol_hash_smallworldindexbr', 'B16R2'),
            ('mol_hash_smallworldindexbrl', 'B16R2L9'),
            ('mol_hash_arthorsubstructureorder', '000f001001000c000300005f000000'),
        ]

        for function, expected in tests:
            hash = c.execute(f"select {function}('{smiles}')").fetchone()[0]
            self.assertEqual(hash, expected, f'{function} test failed.')

if __name__=="__main__":
    suite = unittest.TestLoader().loadTestsFromTestCase(TestMolHash)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    errors = len(result.errors)
    failures = len(result.failures)
    sys.exit(errors + failures)
