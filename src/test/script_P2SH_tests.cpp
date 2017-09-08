// Copyright (c) 2012-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "key.h"
#include "keystore.h"
#include "validation.h"
#include "policy/policy.h"
#include "script/script.h"
#include "script/script_error.h"
#include "script/sign.h"
#include "test/test_ebakus.h"

#ifdef ENABLE_WALLET
#include "wallet/wallet_ismine.h"
#endif

#include <vector>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(script_P2SH_tests, BasicTestingSetup)

BOOST_AUTO_TEST_SUITE_END()
