#ifndef ALIAS_H
#define ALIAS_H

#include "rpcserver.h"
#include "dbwrapper.h"
#include "script/script.h"
#include "serialize.h"

class CWalletTx;
class CTransaction;
class CReserveKey;
class CValidationState;
class CCoinsViewCache;
class CCoins;
class CRecipient;
class CAliasIndex {
public:
    uint256 txHash;
    int64_t nHeight;
    std::vector<unsigned char> vchValue;
	std::vector<unsigned char> vchPubKey;
    CAliasIndex() { 
        SetNull();
    }
    CAliasIndex(const CTransaction &tx) {
        SetNull();
        UnserializeFromTx(tx);
    }
	void ClearAlias()
	{
		vchValue.clear();
		vchPubKey.clear();
	}
	ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {        
		READWRITE(txHash);
        READWRITE(VARINT(nHeight));
    	READWRITE(vchValue);
		READWRITE(vchPubKey);
	}

    friend bool operator==(const CAliasIndex &a, const CAliasIndex &b) {
        return (a.nHeight == b.nHeight && a.txHash == b.txHash && a.vchValue == b.vchValue && a.vchPubKey == b.vchPubKey);
    }

    friend bool operator!=(const CAliasIndex &a, const CAliasIndex &b) {
        return !(a == b);
    }
    
    void SetNull() { txHash.IsNull(); nHeight = 0; vchValue.clear(); vchPubKey.clear(); }
    bool IsNull() const { return (nHeight == 0 && txHash.IsNull() && vchValue.empty() && vchPubKey.empty()); }
	bool UnserializeFromTx(const CTransaction &tx);
	const std::vector<unsigned char> Serialize();
};

class CAliasDB : public CDBWrapper {
public:
    CAliasDB(size_t nCacheSize, bool fMemory, bool fWipe) : CDBWrapper(GetDataDir() / "aliases", nCacheSize, fMemory, fWipe) {
    }

	bool WriteAlias(const std::vector<unsigned char>& name, std::vector<CAliasIndex>& vtxPos) {
		return Write(make_pair(std::string("namei"), name), vtxPos);
	}

	bool EraseAlias(const std::vector<unsigned char>& name) {
	    return Erase(make_pair(std::string("namei"), name));
	}
	bool ReadAlias(const std::vector<unsigned char>& name, std::vector<CAliasIndex>& vtxPos) {
		return Read(make_pair(std::string("namei"), name), vtxPos);
	}
	bool ExistsAlias(const std::vector<unsigned char>& name) {
	    return Exists(make_pair(std::string("namei"), name));
	}

    bool ScanNames(
            const std::vector<unsigned char>& vchName,
            unsigned int nMax,
            std::vector<std::pair<std::vector<unsigned char>, CAliasIndex> >& nameScan);

    bool ReconstructAliasIndex(CBlockIndex *pindexRescan);
};

class COfferDB;
class CCertDB;
class CEscrowDB;
class CMessageDB;
extern CAliasDB *paliasdb;
extern COfferDB *pofferdb;
extern CCertDB *pcertdb;
extern CEscrowDB *pescrowdb;
extern CMessageDB *pmessagedb;



std::string stringFromVch(const std::vector<unsigned char> &vch);
std::vector<unsigned char> vchFromValue(const UniValue& value);
std::vector<unsigned char> vchFromString(const std::string &str);
std::string stringFromValue(const UniValue& value);
int GetSyscoinTxVersion();
const int SYSCOIN_TX_VERSION = 0x7400;
static const unsigned int MAX_NAME_LENGTH = 255;
static const unsigned int MAX_VALUE_LENGTH = 1023;
static const unsigned int MAX_ID_LENGTH = 20;
static const unsigned int MAX_ENCRYPTED_VALUE_LENGTH = 1108;

bool CheckAliasInputs(const CTransaction &tx, CValidationState &state,
	const CCoinsViewCache &inputs, bool fBlock, bool fMiner, bool fJustCheck, int nHeight);
void CreateRecipient(const CScript& scriptPubKey, CRecipient& recipient);
void CreateFeeRecipient(const CScript& scriptPubKey, const std::vector<unsigned char>& data, CRecipient& recipient);
bool IsAliasMine(const CTransaction& tx);
bool IsAliasOp(int op);


bool GetTxOfAlias(const std::vector<unsigned char> &vchName, CTransaction& tx);
int IndexOfAliasOutput(const CTransaction& tx);
bool GetAliasOfTx(const CTransaction& tx, std::vector<unsigned char>& name);
bool DecodeAliasTx(const CTransaction& tx, int& op, int& nOut, std::vector<std::vector<unsigned char> >& vvch, int nHeight);
bool DecodeAliasScript(const CScript& script, int& op, std::vector<std::vector<unsigned char> > &vvch);
bool DecodeAliasScript(const CScript& script, int& op,
		std::vector<std::vector<unsigned char> > &vvch, CScript::const_iterator& pc);
bool GetAliasAddress(const CTransaction& tx, std::string& strAddress);
void GetAliasValue(const std::string& strName, std::string& strAddress);
CAmount convertCurrencyCodeToSyscoin(const std::vector<unsigned char> &vchCurrencyCode, const float &nPrice, const unsigned int &nHeight, int &precision);
bool HasReachedMainNetForkB2();
bool ExistsInMempool(const std::vector<unsigned char> &vchToFind, opcodetype type);
unsigned int QtyOfPendingAcceptsInMempool(const std::vector<unsigned char>& vchToFind);
std::string getCurrencyToSYSFromAlias(const std::vector<unsigned char> &vchCurrency, CAmount &nFee, const unsigned int &nHeightToFind, std::vector<std::string>& rateList, int &precision);
std::string aliasFromOp(int op);
bool IsAliasOp(int op);
int GetAliasExpirationDepth();
CScript RemoveAliasScriptPrefix(const CScript& scriptIn);
int GetSyscoinDataOutput(const CTransaction& tx);
bool GetSyscoinData(const CTransaction &tx, std::vector<unsigned char> &vchData);
#endif // ALIAS_H
