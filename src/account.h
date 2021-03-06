#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <unordered_map>

#include "common.h"
#include "serialize.h"
#include "pubkey.h"
#include "crypto/hash.h"
#include "amount.h"

class CAccount
{
public:
    CAccount();

    U256 GetBalance() const { return mBalance; }
    void SetBalance(U256 balance) { mBalance = balance; }

    void AddBalance(CAmount amount) { mBalance += amount; }
    void SubBalance(CAmount amount) { mBalance -= amount; }

    U256 GetSequence() const { return mSequence; }
    void IncSequence() { ++mSequence; }

    H256 GetStorageRoot() const { return mStorageRoot; }
    void SetStorageRoot(const H256& hash) { mStorageRoot = hash; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(VARINT(mBalance));
        READWRITE(VARINT(mSequence));
        READWRITE(mStorageRoot);
        READWRITE(mCode);
    }

private:
    U256 mBalance;
    U256 mSequence;
    H256 mStorageRoot;
    Bytes mCode;
};

using CAccountMap = std::unordered_map<CKeyID, CAccount, CKeyID::hash>;

#endif // ACCOUNT_H
