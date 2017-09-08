// Copyright (c) 2011-2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "key.h"
#include "keystore.h"
#include "policy/policy.h"
#include "script/script.h"
#include "script/script_error.h"
#include "script/interpreter.h"
#include "script/sign.h"
#include "uint256.h"
#include "test/test_ebakus.h"

#ifdef ENABLE_WALLET
#include "wallet/wallet_ismine.h"
#endif

#include <boost/foreach.hpp>
#include <boost/test/unit_test.hpp>

using namespace std;

typedef vector<unsigned char> valtype;

BOOST_FIXTURE_TEST_SUITE(multisig_tests, BasicTestingSetup)



BOOST_AUTO_TEST_SUITE_END()
