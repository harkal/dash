// Copyright (c) 2013-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "consensus/validation.h"
#include "data/sighash.json.h"
#include "hash.h"
#include "validation.h" // For CheckTransaction
#include "random.h"
#include "script/interpreter.h"
#include "script/script.h"
#include "serialize.h"
#include "streams.h"
#include "test/test_ebakus.h"
#include "util.h"
#include "utilstrencodings.h"
#include "version.h"

#include <iostream>

#include <boost/test/unit_test.hpp>

#include <univalue.h>

BOOST_FIXTURE_TEST_SUITE(sighash_tests, BasicTestingSetup)

BOOST_AUTO_TEST_SUITE_END()
