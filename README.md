  DISCLAIMER
===============================
This is part of my experimentation with adding scripting to Dash and making it close to something like Ethereum. Changing the system from unspent output oriented to account based proved too much work at the end and not much of the masternode code could be reused so I had to abandon the attempt, and go to back to the drawing board. **However** there is a lot of good and probably valuable work that went into this, so I decided to share it in the hope that it will be useful to some people.

# One might find useful: 
* the implementation of the Κeccak256 hashing algorithm
* the hugely improved big number handling classes
* the appropriate object oriented hashing, address handling, etc
* the implementation of the Merkle Patricia Trees
* the adaptation of the Dash code to use the above facilities


The diffstat to the main Dash Code is quite huge:

```
 configure.ac                         |    2 +-
 src/Makefile.am                      |   34 ++-
 src/account.cpp                      |    6 +
 src/account.h                        |   48 ++++
 src/addrdb.cpp                       |   20 +-
 src/addressindex.h                   |   14 +-
 src/addrman.cpp                      |    8 +-
 src/addrman.h                        |    8 +-
 src/alert.cpp                        |   12 +-
 src/alert.h                          |    9 +-
 src/base58.cpp                       |    5 +-
 src/bloom.cpp                        |   73 +-----
 src/bloom.h                          |    9 +-
 src/chain.cpp                        |    2 +-
 src/chain.h                          |   28 +--
 src/chainparams.cpp                  |   68 ++++--
 src/coincontrol.h                    |    2 +-
 src/coins.cpp                        |   59 ++---
 src/coins.h                          |   66 +++--
 src/common.cpp                       |    7 +
 src/common.h                         |   67 ++++++
 src/consensus/merkle.cpp             |   32 +--
 src/consensus/merkle.h               |   11 +-
 src/consensus/params.h               |   12 +-
 src/core_memusage.h                  |   20 +-
 src/core_write.cpp                   |   35 ---
 src/crypto/hash.cpp                  |  185 ++++++++++++++
 src/crypto/hash.h                    |  187 +++++++++++++++
 src/crypto/keccak256.cpp             |   40 ++++
 src/crypto/keccak256.h               |   34 +++
 src/ebakus-tx.cpp                    |   55 -----
 src/executor.cpp                     |   26 ++
 src/executor.h                       |   18 ++
 src/flat-database.h                  |   10 +-
 src/governance-classes.cpp           |  118 +--------
 src/governance-classes.h             |    8 +-
 src/governance-object.cpp            |   15 +-
 src/governance-object.h              |   10 +-
 src/governance-vote.cpp              |    2 +-
 src/governance-vote.h                |   10 +-
 src/governance-votedb.cpp            |    6 +-
 src/governance-votedb.h              |    6 +-
 src/governance.cpp                   |   79 +++---
 src/governance.h                     |   48 ++--
 src/hash.h                           |   56 ++---
 src/hdchain.cpp                      |    2 +-
 src/hdchain.h                        |    8 +-
 src/init.cpp                         |   19 +-
 src/instantx.cpp                     |  285 +++++-----------------
 src/instantx.h                       |   48 ++--
 src/key.cpp                          |    6 +-
 src/key.h                            |    4 +-
 src/masternode-payments.cpp          |   32 +--
 src/masternode-payments.h            |   14 +-
 src/masternode.cpp                   |   28 ++-
 src/masternode.h                     |   16 +-
 src/masternodeman.cpp                |   56 ++---
 src/masternodeman.h                  |   30 +--
 src/merkleblock.cpp                  |   46 ++--
 src/merkleblock.h                    |   16 +-
 src/messagesigner.cpp                |    4 +-
 src/messagesigner.h                  |    4 +-
 src/miner.cpp                        |   64 +++--
 src/miner.h                          |    3 +-
 src/net.cpp                          |   18 +-
 src/net.h                            |   14 +-
 src/net_processing.cpp               |  123 +++++-----
 src/netbase.cpp                      |    2 +-
 src/policy/fees.cpp                  |    6 +-
 src/policy/fees.h                    |    5 +-
 src/policy/policy.cpp                |   99 --------
 src/policy/rbf.cpp                   |    6 +-
 src/pow.cpp                          |   49 ++--
 src/pow.h                            |    4 +-
 src/primitives/block.cpp             |    2 +-
 src/primitives/block.h               |   26 +-
 src/primitives/transaction.cpp       |  138 +++++++----
 src/primitives/transaction.h         |  363 +++++++++++++++-------------
 src/privatesend-client.cpp           |   71 +-----
 src/privatesend-server.cpp           |  674 +--------------------------------------------------
 src/privatesend.cpp                  |   31 +--
 src/privatesend.h                    |    4 +-
 src/protocol.cpp                     |    4 +-
 src/protocol.h                       |    7 +-
 src/pubkey.cpp                       |    6 +-
 src/pubkey.h                         |   28 ++-
 src/qt/clientmodel.cpp               |    4 +-
 src/qt/coincontroldialog.cpp         |   19 +-
 src/qt/transactiondesc.cpp           |   13 +-
 src/qt/transactionrecord.cpp         |   22 +-
 src/qt/transactionrecord.h           |   10 +-
 src/qt/transactiontablemodel.cpp     |   26 +-
 src/qt/walletmodel.cpp               |   17 +-
 src/qt/walletmodel.h                 |    3 +-
 src/qt/walletmodeltransaction.cpp    |    4 +-
 src/rest.cpp                         |    6 +-
 src/rpc/blockchain.cpp               |   38 ++-
 src/rpc/mining.cpp                   |   44 ++--
 src/rpc/misc.cpp                     |   23 +-
 src/rpc/rawtransaction.cpp           |  110 +--------
 src/script/ebakusconsensus.cpp       |   22 --
 src/script/ebakusconsensus.h         |    8 -
 src/script/interpreter.cpp           |  212 +----------------
 src/script/interpreter.h             |    4 +-
 src/script/script.h                  |    1 +
 src/script/sigcache.cpp              |   18 +-
 src/script/sigcache.h                |    2 +-
 src/script/sign.cpp                  |   28 +--
 src/script/standard.cpp              |    7 +-
 src/script/standard.h                |    6 +-
 src/serialize.h                      |    2 +-
 src/spentindex.h                     |   11 +-
 src/spork.cpp                        |    4 +-
 src/spork.h                          |    4 +-
 src/state.cpp                        |   64 +++++
 src/state.h                          |   36 +++
 src/test/DoS_tests.cpp               |   81 -------
 src/test/addrman_tests.cpp           |   10 +-
 src/test/base58_tests.cpp            |    8 +-
 src/test/bloom_tests.cpp             |   22 +-
 src/test/coins_tests.cpp             |  276 +--------------------
 src/test/hash_tests.cpp              |    1 +
 src/test/key_tests.cpp               |    6 +-
 src/test/mempool_tests.cpp           |  459 -----------------------------------
 src/test/merkle_tests.cpp            |   22 +-
 src/test/miner_tests.cpp             |  365 ----------------------------
 src/test/multisig_tests.cpp          |  290 ----------------------
 src/test/pmt_tests.cpp               |   18 +-
 src/test/policyestimator_tests.cpp   |  194 ---------------
 src/test/pow_tests.cpp               |    3 +-
 src/test/rpc_wallet_tests.cpp        |    2 +-
 src/test/script_P2SH_tests.cpp       |  350 ---------------------------
 src/test/script_tests.cpp            | 1126 --------------------------------------------------------------------------------------
 src/test/sighash_tests.cpp           |  190 ---------------
 src/test/skiplist_tests.cpp          |   14 +-
 src/test/test_ebakus.cpp             |    4 +-
 src/test/transaction_tests.cpp       |  372 -----------------------------
 src/test/txvalidationcache_tests.cpp |   65 -----
 src/triedb/exceptions.h              |   21 ++
 src/triedb/nibble.cpp                |  111 +++++++++
 src/triedb/nibble.h                  |  151 ++++++++++++
 src/triedb/triedb.cpp                |    8 +
 src/triedb/triedb.h                  |  466 ++++++++++++++++++++++++++++++++++++
 src/txdb.cpp                         |   30 +--
 src/txdb.h                           |   18 +-
 src/txmempool.cpp                    |  216 +++--------------
 src/txmempool.h                      |   42 ++--
 src/ui_interface.h                   |    4 +-
 src/validation.cpp                   |  793 +++++-------------------------------------------------------
 src/validation.h                     |   85 +++----
 src/validationinterface.cpp          |    6 +-
 src/validationinterface.h            |   20 +-
 src/wallet/crypter.cpp               |    4 +-
 src/wallet/rpcwallet.cpp             |   68 +++---
 src/wallet/test/wallet_tests.cpp     |  334 --------------------------
 src/wallet/wallet.cpp                |  833 +++++++++------------------------------------------------------
 src/wallet/wallet.h                  |   56 ++---
 src/wallet/walletdb.cpp              |   28 +--
 src/wallet/walletdb.h                |   16 +-
 159 files changed, 3173 insertions(+), 8718 deletions(-)
 create mode 100644 src/account.cpp
 create mode 100644 src/account.h
 create mode 100644 src/common.cpp
 create mode 100644 src/common.h
 create mode 100644 src/crypto/hash.cpp
 create mode 100644 src/crypto/hash.h
 create mode 100644 src/crypto/keccak256.cpp
 create mode 100644 src/crypto/keccak256.h
 create mode 100644 src/executor.cpp
 create mode 100644 src/executor.h
 create mode 100644 src/state.cpp
 create mode 100644 src/state.h
 create mode 100644 src/triedb/exceptions.h
 create mode 100644 src/triedb/nibble.cpp
 create mode 100644 src/triedb/nibble.h
 create mode 100644 src/triedb/triedb.cpp
 create mode 100644 src/triedb/triedb.h
``` 


Ebakus Core staging tree 0.12.1
===============================

`master:` [![Build Status](https://travis-ci.org/ebakuspay/ebakus.svg?branch=master)](https://travis-ci.org/ebakuspay/ebakus) `v0.12.0.x:` [![Build Status](https://travis-ci.org/ebakuspay/ebakus.svg?branch=v0.12.0.x)](https://travis-ci.org/ebakuspay/ebakus/branches) `v0.12.1.x:` [![Build Status](https://travis-ci.org/ebakuspay/ebakus.svg?branch=v0.12.1.x)](https://travis-ci.org/ebakuspay/ebakus/branches)

https://www.ebakus.com


What is Ebakus?
----------------

Ebakus is an experimental new digital currency that enables anonymous, instant
payments to anyone, anywhere in the world. Ebakus uses peer-to-peer technology
to operate with no central authority: managing transactions and issuing money
are carried out collectively by the network. Ebakus Core is the name of the open
source software which enables the use of this currency.

For more information, as well as an immediately useable, binary version of
the Ebakus Core software, see https://www.ebakus.org/get-ebakus/.


License
-------

Ebakus Core is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see https://opensource.org/licenses/MIT.

Development Process
-------------------

The `master` branch is meant to be stable. Development is normally done in separate branches.
[Tags](https://github.com/ebakuspay/ebakus/tags) are created to indicate new official,
stable release versions of Ebakus Core.

The contribution workflow is described in [CONTRIBUTING.md](CONTRIBUTING.md).

Testing
-------

Testing and code review is the bottleneck for development; we get more pull
requests than we can review and test on short notice. Please be patient and help out by testing
other people's pull requests, and remember this is a security-critical project where any mistake might cost people
lots of money.

### Automated Testing

Developers are strongly encouraged to write [unit tests](/doc/unit-tests.md) for new code, and to
submit new unit tests for old code. Unit tests can be compiled and run
(assuming they weren't disabled in configure) with: `make check`

There are also [regression and integration tests](/qa) of the RPC interface, written
in Python, that are run automatically on the build server.
These tests can be run (if the [test dependencies](/qa) are installed) with: `qa/pull-tester/rpc-tests.py`

The Travis CI system makes sure that every pull request is built for Windows
and Linux, OS X, and that unit and sanity tests are automatically run.

### Manual Quality Assurance (QA) Testing

Changes should be tested by somebody other than the developer who wrote the
code. This is especially important for large or high-risk changes. It is useful
to add a test plan to the pull request description if testing the changes is
not straightforward.

Translations
------------

Changes to translations as well as new translations can be submitted to
[Ebakus Core's Transifex page](https://www.transifex.com/projects/p/ebakus/).

Translations are periodically pulled from Transifex and merged into the git repository. See the
[translation process](doc/translation_process.md) for details on how this works.

**Important**: We do not accept translation changes as GitHub pull requests because the next
pull from Transifex would automatically overwrite them again.

Translators should also follow the [forum](https://www.ebakus.org/forum/topic/ebakus-worldwide-collaboration.88/).
