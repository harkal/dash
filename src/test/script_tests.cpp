// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "data/script_tests.json.h"

#include "core_io.h"
#include "key.h"
#include "keystore.h"
#include "script/script.h"
#include "script/script_error.h"
#include "script/sign.h"
#include "util.h"
#include "utilstrencodings.h"
#include "test/test_ebakus.h"

#if defined(HAVE_CONSENSUS_LIB)
#include "script/ebakusconsensus.h"
#endif

#include <fstream>
#include <stdint.h>
#include <string>
#include <vector>

#include <boost/foreach.hpp>
#include <boost/test/unit_test.hpp>

#include <univalue.h>

using namespace std;

