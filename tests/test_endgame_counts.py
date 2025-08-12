from scripts.endgame_counts import REFERENCE_COUNTS

def test_reference_table_has_expected_keys():
    assert sorted(REFERENCE_COUNTS.keys()) == [5,10,15,20,25,30,35,40,45,50]

def test_reference_numbers_are_positive():
    assert all(v > 0 for v in REFERENCE_COUNTS.values())
