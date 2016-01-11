#include "cert.h"
#include "alias.h"
#include "offer.h"
#include "init.h"
#include "main.h"
#include "util.h"
#include "random.h"
#include "base58.h"
#include "rpcserver.h"
#include "wallet/wallet.h"
#include "chainparams.h"
#include "messagecrypter.h"
#include <boost/algorithm/hex.hpp>
#include <boost/xpressive/xpressive_dynamic.hpp>
#include <boost/foreach.hpp>
#include <boost/thread.hpp>
using namespace std;


extern void SendMoneySyscoin(const vector<CRecipient> &vecSend, CAmount nValue, bool fSubtractFeeFromAmount, CWalletTx& wtxNew, const CWalletTx* wtxIn=NULL, bool syscoinTx=true);
bool EncryptMessage(const vector<unsigned char> &vchPubKey, const vector<unsigned char> &vchMessage, string &strCipherText)
{
	CMessageCrypter crypter;
	if(!crypter.Encrypt(stringFromVch(vchPubKey), stringFromVch(vchMessage), strCipherText))
		return false;

	return true;
}
bool DecryptMessage(const vector<unsigned char> &vchPubKey, const vector<unsigned char> &vchCipherText, string &strMessage)
{
	std::vector<unsigned char> vchPubKeyByte;
	CKey PrivateKey;
    boost::algorithm::unhex(vchPubKey.begin(), vchPubKey.end(), std::back_inserter(vchPubKeyByte));
	CPubKey PubKey(vchPubKeyByte);
	CKeyID pubKeyID = PubKey.GetID();
	if (!pwalletMain->GetKey(pubKeyID, PrivateKey))
        return false;
	CSyscoinSecret Secret(PrivateKey);
	PrivateKey = Secret.GetKey();
	std::vector<unsigned char> vchPrivateKey(PrivateKey.begin(), PrivateKey.end());
	CMessageCrypter crypter;
	if(!crypter.Decrypt(HexStr(vchPrivateKey), stringFromVch(vchCipherText), strMessage))
		return false;
	
	return true;
}
void PutToCertList(std::vector<CCert> &certList, CCert& index) {
	int i = certList.size() - 1;
	BOOST_REVERSE_FOREACH(CCert &o, certList) {
        if(index.nHeight != 0 && o.nHeight == index.nHeight) {
        	certList[i] = index;
            return;
        }
        else if(!o.txHash.IsNull() && o.txHash == index.txHash) {
        	certList[i] = index;
            return;
        }
        i--;
	}
    certList.push_back(index);
}
bool IsCertOp(int op) {
    return op == OP_CERT_ACTIVATE
        || op == OP_CERT_UPDATE
        || op == OP_CERT_TRANSFER;
}

// Increase expiration to 36000 gradually starting at block 24000.
// Use for validation purposes and pass the chain height.
int GetCertExpirationDepth() {
    return 525600;
}


string certFromOp(int op) {
    switch (op) {
    case OP_CERT_ACTIVATE:
        return "certactivate";
    case OP_CERT_UPDATE:
        return "certupdate";
    case OP_CERT_TRANSFER:
        return "certtransfer";
    default:
        return "<unknown cert op>";
    }
}
bool CCert::UnserializeFromTx(const CTransaction &tx) {
	vector<unsigned char> vchData;
	if(!GetSyscoinData(tx, vchData))
		return false;
    try {
        CDataStream dsCert(vchData, SER_NETWORK, PROTOCOL_VERSION);
        dsCert >> *this;
    } catch (std::exception &e) {
        return false;
    }
    return true;
}
const vector<unsigned char> CCert::Serialize() {
    CDataStream dsCert(SER_NETWORK, PROTOCOL_VERSION);
    dsCert << *this;
    const vector<unsigned char> vchData(dsCert.begin(), dsCert.end());
    return vchData;

}

//TODO implement
bool CCertDB::ScanCerts(const std::vector<unsigned char>& vchCert, unsigned int nMax,
        std::vector<std::pair<std::vector<unsigned char>, CCert> >& certScan) {

    CDBIterator *pcursor = pcertdb->NewIterator();

    CDataStream ssKeySet(SER_DISK, CLIENT_VERSION);
    ssKeySet << make_pair(string("certi"), vchCert);
    pcursor->Seek(ssKeySet.str());

    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
		pair<string, vector<unsigned char> > key;
        try {
            if (pcursor->GetKey(key) && key.first == "certi") {
                vector<unsigned char> vchCert = key.second;
                vector<CCert> vtxPos;
                pcursor->GetValue(vtxPos);
                CCert txPos;
                if (!vtxPos.empty())
                    txPos = vtxPos.back();
                certScan.push_back(make_pair(vchCert, txPos));
            }
            if (certScan.size() >= nMax)
                break;

            pcursor->Next();
        } catch (std::exception &e) {
            return error("%s() : deserialize error", __PRETTY_FUNCTION__);
        }
    }
    delete pcursor;
    return true;
}

/**
 * [CCertDB::ReconstructCertIndex description]
 * @param  pindexRescan [description]
 * @return              [description]
 */
bool CCertDB::ReconstructCertIndex(CBlockIndex *pindexRescan) {
    CBlockIndex* pindex = pindexRescan;
	if(!HasReachedMainNetForkB2())
		return true;
    {
    TRY_LOCK(pwalletMain->cs_wallet, cs_trylock);
    while (pindex) {

        int nHeight = pindex->nHeight;
        CBlock block;
        ReadBlockFromDisk(block, pindex, Params().GetConsensus());
        uint256 txblkhash;

        BOOST_FOREACH(CTransaction& tx, block.vtx) {

            if (tx.nVersion != SYSCOIN_TX_VERSION)
                continue;

            vector<vector<unsigned char> > vvchArgs;
            int op, nOut;

            // decode the cert op, params, height
            bool o = DecodeCertTx(tx, op, nOut, vvchArgs, -1);
            if (!o || !IsCertOp(op)) continue;

            vector<unsigned char> vchCert = vvchArgs[0];

            // get the transaction
            if(!GetTransaction(tx.GetHash(), tx, Params().GetConsensus(), txblkhash, true))
                continue;

            // attempt to read cert from txn
            CCert txCert;
            CCert txCA;
            if(!txCert.UnserializeFromTx(tx))
                return error("ReconstructCertIndex() : failed to unserialize cert from tx");

            // save serialized cert
            CCert serializedCert = txCert;

            // read cert from DB if it exists
            vector<CCert> vtxPos;
            if (ExistsCert(vchCert)) {
                if (!ReadCert(vchCert, vtxPos))
                    return error("ReconstructCertIndex() : failed to read cert from DB");
            }

            txCert.txHash = tx.GetHash();
            txCert.nHeight = nHeight;

            PutToCertList(vtxPos, txCert);

            if (!WriteCert(vchCert, vtxPos))
                return error("ReconstructCertIndex() : failed to write to cert DB");

          
            LogPrintf( "RECONSTRUCT CERT: op=%s cert=%s title=%s hash=%s height=%d\n",
                    certFromOp(op).c_str(),
                    stringFromVch(vvchArgs[0]).c_str(),
                    stringFromVch(txCert.vchTitle).c_str(),
                    tx.GetHash().ToString().c_str(),
                    nHeight);
        }
        pindex = chainActive.Next(pindex);
        
    }
    }
    return true;
}
int IndexOfCertOutput(const CTransaction& tx) {
    vector<vector<unsigned char> > vvch;
    int op, nOut;
    if (!DecodeCertTx(tx, op, nOut, vvch, -1))
        throw runtime_error("IndexOfCertOutput() : cert output not found");
    return nOut;
}

bool IsCertMine(const CTransaction& tx) {
    if (tx.nVersion != SYSCOIN_TX_VERSION)
        return false;

    vector<vector<unsigned char> > vvch;
    int op, nOut;

    bool good = DecodeCertTx(tx, op, nOut, vvch, -1);
    if (!good) 
        return false;
    
    if(!IsCertOp(op))
        return false;

    const CTxOut& txout = tx.vout[nOut];
    if (pwalletMain->IsMine(txout)) {
        return true;
    }
    return false;
}


bool GetTxOfCert(CCertDB& dbCert, const vector<unsigned char> &vchCert,
        CCert& txPos, CTransaction& tx) {
    vector<CCert> vtxPos;
    if (!pcertdb->ReadCert(vchCert, vtxPos) || vtxPos.empty())
        return false;
    txPos = vtxPos.back();
    int nHeight = txPos.nHeight;
    if (nHeight + GetCertExpirationDepth()
            < chainActive.Tip()->nHeight) {
        string cert = stringFromVch(vchCert);
        LogPrintf("GetTxOfCert(%s) : expired", cert.c_str());
        return false;
    }

    uint256 hashBlock;
    if (!GetTransaction(txPos.txHash, tx, Params().GetConsensus(), hashBlock, true))
        return error("GetTxOfCert() : could not read tx from disk");

    return true;
}

bool DecodeCertTxInputs(const CTransaction& tx, int& op, int& nOut,
        vector<vector<unsigned char> >& vvch, CCoinsViewCache &inputs) {
    bool found = false;

	const COutPoint *prevOutput = NULL;
	CCoins prevCoins;
    // Strict check - bug disallowed
    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        const CTxIn& in = tx.vin[i];
		prevOutput = &in.prevout;
		inputs.GetCoins(prevOutput->hash, prevCoins);
        vector<vector<unsigned char> > vvchRead;
        if (DecodeCertScript(prevCoins.vout[prevOutput->n].scriptPubKey, op, vvchRead)) {
            nOut = i; found = true; vvch = vvchRead;
            break;
        }
    }
    if (!found) vvch.clear();
    return found && IsCertOp(op);
}
bool DecodeCertTx(const CTransaction& tx, int& op, int& nOut,
        vector<vector<unsigned char> >& vvch, int nHeight) {
    bool found = false;


    // Strict check - bug disallowed
    for (unsigned int i = 0; i < tx.vout.size(); i++) {
        const CTxOut& out = tx.vout[i];
        vector<vector<unsigned char> > vvchRead;
        if (DecodeCertScript(out.scriptPubKey, op, vvchRead)) {
            nOut = i; found = true; vvch = vvchRead;
            break;
        }
    }
    if (!found) vvch.clear();
    return found && IsCertOp(op);
}


bool DecodeCertScript(const CScript& script, int& op,
        vector<vector<unsigned char> > &vvch) {
    CScript::const_iterator pc = script.begin();
    return DecodeCertScript(script, op, vvch, pc);
}

bool DecodeCertScript(const CScript& script, int& op,
        vector<vector<unsigned char> > &vvch, CScript::const_iterator& pc) {
    opcodetype opcode;
	vvch.clear();
    if (!script.GetOp(pc, opcode)) return false;
    if (opcode < OP_1 || opcode > OP_16) return false;
    op = CScript::DecodeOP_N(opcode);

    for (;;) {
        vector<unsigned char> vch;
        if (!script.GetOp(pc, opcode, vch))
            return false;
        if (opcode == OP_DROP || opcode == OP_2DROP || opcode == OP_NOP)
            break;
        if (!(opcode >= 0 && opcode <= OP_PUSHDATA4))
            return false;
        vvch.push_back(vch);
    }

    // move the pc to after any DROP or NOP
    while (opcode == OP_DROP || opcode == OP_2DROP || opcode == OP_NOP) {
        if (!script.GetOp(pc, opcode))
            break;
    }

    pc--;

    if ((op == OP_CERT_ACTIVATE && vvch.size() == 1)
        || (op == OP_CERT_UPDATE && vvch.size() == 1)
        || (op == OP_CERT_TRANSFER && vvch.size() == 1))
        return true;
    return false;
}

bool GetCertAddress(const CTransaction& tx, std::string& strAddress) {
    int op, nOut = 0;
    vector<vector<unsigned char> > vvch;

    if (!DecodeCertTx(tx, op, nOut, vvch, -1))
        return error("GetCertAddress() : could not decode cert tx.");

    const CTxOut& txout = tx.vout[nOut];

    const CScript& scriptPubKey = RemoveCertScriptPrefix(txout.scriptPubKey);
	CTxDestination dest;
	ExtractDestination(scriptPubKey, dest);
	strAddress = CSyscoinAddress(dest).ToString();
    return true;
}

CScript RemoveCertScriptPrefix(const CScript& scriptIn) {
    int op;
    vector<vector<unsigned char> > vvch;
    CScript::const_iterator pc = scriptIn.begin();

    if (!DecodeCertScript(scriptIn, op, vvch, pc))
        throw runtime_error(
                "RemoveCertScriptPrefix() : could not decode cert script");
	
    return CScript(pc, scriptIn.end());
}

bool CheckCertInputs(const CTransaction &tx,
        CValidationState &state, const CCoinsViewCache &inputs, bool fBlock, bool fMiner,
        bool fJustCheck, int nHeight) {

    if (!tx.IsCoinBase()) {
			LogPrintf("*** %d %d %s %s %s %s\n", nHeight,
				chainActive.Tip()->nHeight, tx.GetHash().ToString().c_str(),
				fBlock ? "BLOCK" : "", fMiner ? "MINER" : "",
				fJustCheck ? "JUSTCHECK" : "");

        bool found = false;
        const COutPoint *prevOutput = NULL;
        CCoins prevCoins;

        int prevOp;
        vector<vector<unsigned char> > vvchPrevArgs;
		vvchPrevArgs.clear();
        // Strict check - bug disallowed
		for (unsigned int i = 0; i < tx.vin.size(); i++) {
			vector<vector<unsigned char> > vvch, vvch2;
			prevOutput = &tx.vin[i].prevout;
			inputs.GetCoins(prevOutput->hash, prevCoins);
			if(DecodeCertScript(prevCoins.vout[prevOutput->n].scriptPubKey, prevOp, vvch))
			{
				vvchPrevArgs = vvch;
				found = true;
				break;
			}
			else if(DecodeOfferScript(prevCoins.vout[prevOutput->n].scriptPubKey, prevOp, vvch2))
			{
				found = true; 
				vvchPrevArgs = vvch2;
				break;
			}
			if(!found)vvchPrevArgs.clear();
			
		}
		
        // Make sure cert outputs are not spent by a regular transaction, or the cert would be lost
        if (tx.nVersion != SYSCOIN_TX_VERSION) {
            if (found)
                return error(
                        "CheckCertInputs() : a non-syscoin transaction with a syscoin input");
            return true;
        }
        vector<vector<unsigned char> > vvchArgs;
        int op, nOut;
        bool good = DecodeCertTx(tx, op, nOut, vvchArgs, -1);
        if (!good)
            return error("CheckCertInputs() : could not decode a syscoin tx");
        // unserialize cert object from txn, check for valid
        CCert theCert(tx);
        if (theCert.IsNull())
            return error("CheckCertInputs() : null cert object");
		if(theCert.vchData.size() > MAX_ENCRYPTED_VALUE_LENGTH)
		{
			return error("cert data too big");
		}
		if(theCert.vchTitle.size() > MAX_NAME_LENGTH)
		{
			return error("cert title too big");
		}
		if(theCert.vchPubKey.size() > MAX_VALUE_LENGTH)
		{
			return error("cert pub key too big");
		}
        if (vvchArgs[0].size() > MAX_NAME_LENGTH)
            return error("cert hex guid too long");
        switch (op) {
        case OP_CERT_ACTIVATE:
            if (found)
                return error(
                        "CheckCertInputs() : certactivate tx pointing to previous syscoin tx");

            break;

        case OP_CERT_UPDATE:
			// if previous op was a cert or an offeraccept its ok
            if ( !found || (!IsCertOp(prevOp) && !IsOfferOp(prevOp)))
                return error("certupdate previous op is invalid");
            if (vvchPrevArgs[0] != vvchArgs[0] && !IsOfferOp(prevOp))
                return error("CheckCertInputs() : certupdate prev cert mismatch vvchPrevArgs[0]: %s, vvchArgs[0] %s", stringFromVch(vvchPrevArgs[0]).c_str(), stringFromVch(vvchArgs[0]).c_str());

            break;

        

        case OP_CERT_TRANSFER:
            // validate conditions
            if ( !found || !IsCertOp(prevOp))
                return error("certtransfer previous op is invalid");
            if (vvchPrevArgs[0] != vvchArgs[0])
                return error("CheckCertInputs() : certtransfer cert mismatch");
            break;

        default:
            return error( "CheckCertInputs() : cert transaction has unknown op");
        }



        // these ifs are problably total bullshit except for the certnew
        if (fBlock || (!fBlock && !fMiner && !fJustCheck)) {
			// if not an certnew, load the cert data from the DB
			vector<CCert> vtxPos;
			if (pcertdb->ExistsCert(vvchArgs[0]) && !fJustCheck) {
				if (!pcertdb->ReadCert(vvchArgs[0], vtxPos))
					return error(
							"CheckCertInputs() : failed to read from cert DB");
			}
            if (!fMiner && !fJustCheck && chainActive.Tip()->nHeight != nHeight) {
				const CCert& dbCert = vtxPos.back();
				if(theCert.vchData.empty())
					theCert.vchData = dbCert.vchData;
				if(theCert.vchTitle.empty())
					theCert.vchTitle = dbCert.vchTitle;
				if(theCert.vchPubKey.empty())
					theCert.vchPubKey = dbCert.vchPubKey;
                int nHeight = chainActive.Tip()->nHeight;     
				
                // set the cert's txn-dependent values
				theCert.nHeight = chainActive.Tip()->nHeight;
				theCert.txHash = tx.GetHash();
				PutToCertList(vtxPos, theCert);
				{
				TRY_LOCK(cs_main, cs_trymain);
                // write cert  
                if (!pcertdb->WriteCert(vvchArgs[0], vtxPos))
                    return error( "CheckCertInputs() : failed to write to cert DB");

              			
                // debug
				if(fDebug)
					LogPrintf( "CONNECTED CERT: op=%s cert=%s title=%s hash=%s height=%d\n",
                        certFromOp(op).c_str(),
                        stringFromVch(vvchArgs[0]).c_str(),
                        stringFromVch(theCert.vchTitle).c_str(),
                        tx.GetHash().ToString().c_str(),
                        nHeight);
				}
            }
            
        }
    }
    return true;
}


void rescanforcerts(CBlockIndex *pindexRescan) {
    LogPrintf("Scanning blockchain for certs to create fast index...\n");
    pcertdb->ReconstructCertIndex(pindexRescan);
}



UniValue certnew(const UniValue& params, bool fHelp) {
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error(
		"certnew <title> <data> [private=0]\n"
                        "<title> title, 255 bytes max.\n"
                        "<data> data, 1KB max.\n"
						"<private> set to 1 if you only want to make the cert data private, only the owner of the cert can view it. Off by default.\n"
                        + HelpRequiringPassphrase());
	if(!HasReachedMainNetForkB2())
		throw runtime_error("Please wait until B2 hardfork starts in before executing this command.");
	vector<unsigned char> vchTitle = vchFromString(params[0].get_str());
    vector<unsigned char> vchData = vchFromString(params[1].get_str());
	bool bPrivate = false;
    if(vchTitle.size() < 1)
        throw runtime_error("certificate cannot be empty!");

    if(vchTitle.size() > MAX_NAME_LENGTH)
        throw runtime_error("certificate title cannot exceed 255 bytes!");

	if(params.size() >= 3)
	{
		bPrivate = atoi(params[2].get_str().c_str()) == 1? true: false;
	}
    if (vchData.size() < 1)
	{
        vchData = vchFromString(" ");
		bPrivate = false;
	}
    // gather inputs
	int64_t rand = GetRand(std::numeric_limits<int64_t>::max());
	vector<unsigned char> vchRand = CScriptNum(rand).getvch();
    vector<unsigned char> vchCert = vchFromValue(HexStr(vchRand));

    // this is a syscoin transaction
    CWalletTx wtx;

	EnsureWalletIsUnlocked();
    //create certactivate txn keys
    CPubKey newDefaultKey;
    pwalletMain->GetKeyFromPool(newDefaultKey);
    CScript scriptPubKeyOrig;
    scriptPubKeyOrig= GetScriptForDestination(newDefaultKey.GetID());
    CScript scriptPubKey;

    
	std::vector<unsigned char> vchPubKey(newDefaultKey.begin(), newDefaultKey.end());
	string strPubKey = HexStr(vchPubKey);
	if(bPrivate)
	{
		string strCipherText;
		if(!EncryptMessage(vchFromString(strPubKey), vchData, strCipherText))
		{
			throw runtime_error("Could not encrypt certificate data!");
		}
		if (strCipherText.size() > MAX_ENCRYPTED_VALUE_LENGTH)
			throw runtime_error("data length cannot exceed 1023 bytes!");
		vchData = vchFromString(strCipherText);
	}

	// calculate net
    // build cert object
    CCert newCert;
    newCert.vchTitle = vchTitle;
	newCert.vchData = vchData;
	newCert.nHeight = chainActive.Tip()->nHeight;
	newCert.vchPubKey = vchFromString(strPubKey);
	newCert.bPrivate = bPrivate;

    scriptPubKey << CScript::EncodeOP_N(OP_CERT_ACTIVATE) << vchCert << OP_2DROP;
    scriptPubKey += scriptPubKeyOrig;


	// use the script pub key to create the vecsend which sendmoney takes and puts it into vout
	vector<CRecipient> vecSend;
	CRecipient recipient;
	CreateRecipient(scriptPubKey, recipient);
	vecSend.push_back(recipient);

	const vector<unsigned char> &data = newCert.Serialize();
	CScript scriptData;
	scriptData << OP_RETURN << data;
	CRecipient fee;
	CreateFeeRecipient(scriptData, data, fee);
	vecSend.push_back(fee);

	SendMoneySyscoin(vecSend, recipient.nAmount+fee.nAmount, false, wtx);

	UniValue res(UniValue::VARR);
	res.push_back(wtx.GetHash().GetHex());
	res.push_back(HexStr(vchRand));
	return res;
}

UniValue certupdate(const UniValue& params, bool fHelp) {
    if (fHelp || params.size() != 4)
        throw runtime_error(
		"certupdate <guid> <title> <data> <private>\n"
                        "Perform an update on an certificate you control.\n"
                        "<guid> certificate guidkey.\n"
                        "<title> certificate title, 255 bytes max.\n"
                        "<data> certificate data, 1KB max.\n"
						"<private> set to 1 if you only want to make the cert data private, only the owner of the cert can view it.\n"
                        + HelpRequiringPassphrase());
	if(!HasReachedMainNetForkB2())
		throw runtime_error("Please wait until B2 hardfork starts in before executing this command.");
    // gather & validate inputs
    vector<unsigned char> vchCert = vchFromValue(params[0]);
    vector<unsigned char> vchTitle = vchFromValue(params[1]);
    vector<unsigned char> vchData = vchFromValue(params[2]);
	bool bPrivate = atoi(params[3].get_str().c_str()) == 1? true: false;
    if(vchTitle.size() < 1)
        throw runtime_error("certificate title cannot be empty!");

    if(vchTitle.size() > MAX_NAME_LENGTH)
        throw runtime_error("certificate title cannot exceed 255 bytes!");

    if (vchData.size() < 1)
        vchData = vchFromString(" ");

    // this is a syscoind txn
    CWalletTx wtx;
	const CWalletTx* wtxIn;
    CScript scriptPubKeyOrig;

    EnsureWalletIsUnlocked();

    // look for a transaction with this key
    CTransaction tx;
	CCert cert;
    if (!GetTxOfCert(*pcertdb, vchCert, cert, tx))
        throw runtime_error("could not find a certificate with this key");

    // make sure cert is in wallet
	wtxIn = pwalletMain->GetWalletTx(tx.GetHash());
	if (wtxIn == NULL || !IsCertMine(tx))
		throw runtime_error("this cert is not in your wallet");
      	// check for existing cert 's
	if (ExistsInMempool(vchCert, OP_CERT_ACTIVATE) || ExistsInMempool(vchCert, OP_CERT_UPDATE) || ExistsInMempool(vchCert, OP_CERT_TRANSFER)) {
		throw runtime_error("there are pending operations on that cert");
	}
    // unserialize cert object from txn
    CCert theCert;
    if(!theCert.UnserializeFromTx(tx))
        throw runtime_error("cannot unserialize cert from txn");

    // get the cert from DB
    vector<CCert> vtxPos;
    if (!pcertdb->ReadCert(vchCert, vtxPos) || vtxPos.empty())
        throw runtime_error("could not read cert from DB");
    theCert = vtxPos.back();
	CCert copyCert = theCert;
	theCert.ClearCert();
    // get a key from our wallet set dest as ourselves
    CPubKey newDefaultKey;
    pwalletMain->GetKeyFromPool(newDefaultKey);
    scriptPubKeyOrig= GetScriptForDestination(newDefaultKey.GetID());
	std::vector<unsigned char> vchPubKey(newDefaultKey.begin(), newDefaultKey.end());

    // create CERTUPDATE txn keys
    CScript scriptPubKey;
    scriptPubKey << CScript::EncodeOP_N(OP_CERT_UPDATE) << vchCert << OP_2DROP;
    scriptPubKey += scriptPubKeyOrig;
	// if we want to make data private, encrypt it
	if(bPrivate)
	{
		string strCipherText;
		if(!EncryptMessage(copyCert.vchPubKey, vchData, strCipherText))
		{
			throw runtime_error("Could not encrypt certificate data!");
		}
		if (strCipherText.size() > MAX_ENCRYPTED_VALUE_LENGTH)
			throw runtime_error("data length cannot exceed 1023 bytes!");
		vchData = vchFromString(strCipherText);
	}


    if(copyCert.vchTitle != vchTitle)
		theCert.vchTitle = vchTitle;
	if(copyCert.vchData != vchData)
		theCert.vchData = vchData;
	theCert.nHeight = chainActive.Tip()->nHeight;
	theCert.bPrivate = bPrivate;


	vector<CRecipient> vecSend;
	CRecipient recipient;
	CreateRecipient(scriptPubKey, recipient);
	vecSend.push_back(recipient);

	const vector<unsigned char> &data = theCert.Serialize();
	CScript scriptData;
	scriptData << OP_RETURN << data;
	CRecipient fee;
	CreateFeeRecipient(scriptData, data, fee);
	vecSend.push_back(fee);

	SendMoneySyscoin(vecSend, recipient.nAmount+fee.nAmount, false, wtx, wtxIn);	
 	UniValue res(UniValue::VARR);
	res.push_back(wtx.GetHash().GetHex());
	return res;
}


UniValue certtransfer(const UniValue& params, bool fHelp) {
 if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error(
		"certtransfer <certkey> <alias> [offerpurchase=0]\n"
                "<certkey> certificate guidkey.\n"
				"<alias> Alias to transfer this certificate to.\n"
                 + HelpRequiringPassphrase());

	if(!HasReachedMainNetForkB2())
		throw runtime_error("Please wait until B2 hardfork starts in before executing this command.");
    // gather & validate inputs
	vector<unsigned char> vchCert = vchFromValue(params[0]);

	string strAddress = params[1].get_str();
	CPubKey xferKey;
	std::vector<unsigned char> vchPubKey;
	std::vector<unsigned char> vchPubKeyByte;
	try
	{
		vchPubKey = vchFromString(strAddress);		
		boost::algorithm::unhex(vchPubKey.begin(), vchPubKey.end(), std::back_inserter(vchPubKeyByte));
		xferKey  = CPubKey(vchPubKeyByte);
		if(!xferKey.IsValid())
		{
			throw runtime_error("Invalid public key");
		}
	}
	catch(...)
	{
		CSyscoinAddress myAddress = CSyscoinAddress(strAddress);
		if (!myAddress.IsValid())
			throw runtime_error("Invalid syscoin address");
		if (!myAddress.isAlias)
			throw runtime_error("You must transfer to a valid alias");

		// check for alias existence in DB
		vector<CAliasIndex> vtxAliasPos;
		if (!paliasdb->ReadAlias(vchFromString(myAddress.aliasName), vtxAliasPos))
			throw runtime_error("failed to read alias from alias DB");
		if (vtxAliasPos.size() < 1)
			throw runtime_error("no result returned");
		CAliasIndex xferAlias = vtxAliasPos.back();
		vchPubKey = xferAlias.vchPubKey;
		boost::algorithm::unhex(vchPubKey.begin(), vchPubKey.end(), std::back_inserter(vchPubKeyByte));
		xferKey = CPubKey(vchPubKeyByte);
		if(!xferKey.IsValid())
		{
			throw runtime_error("Invalid transfer public key");
		}
	}

	

	bool offerpurchase = false;
	if(params.size() >= 3)
		offerpurchase = atoi(params[2].get_str().c_str()) == 1? true: false;
	CSyscoinAddress sendAddr;
    // this is a syscoin txn
    CWalletTx wtx;
	const CWalletTx* wtxIn;
    CScript scriptPubKeyOrig;

    EnsureWalletIsUnlocked();

    // look for a transaction with this key, also returns
    // an cert object if it is found
    CTransaction tx;
    CCert theCert;
    if (!GetTxOfCert(*pcertdb, vchCert, theCert, tx))
        throw runtime_error("could not find a certificate with this key");

	// check to see if certificate in wallet
	wtxIn = pwalletMain->GetWalletTx(tx.GetHash());
	if (wtxIn == NULL || !IsCertMine(tx))
		throw runtime_error("this certificate is not in your wallet");

	if (ExistsInMempool(vchCert, OP_CERT_TRANSFER)) {
		throw runtime_error("there are pending operations on that cert ");
	}
	// unserialize cert object from txn
	if(!theCert.UnserializeFromTx(tx))
		throw runtime_error("cannot unserialize offer from txn");

	// get the cert from DB
	vector<CCert> vtxPos;
	if (!pcertdb->ReadCert(vchCert, vtxPos) || vtxPos.empty())
		throw runtime_error("could not read cert from DB");
	theCert = vtxPos.back();

	// if cert is private, decrypt the data
	vector<unsigned char> vchData = theCert.vchData;
	if(theCert.bPrivate)
	{		
		string strData;
		string strCipherText;
		
		// decrypt using old key
		if(!DecryptMessage(theCert.vchPubKey, theCert.vchData, strData))
		{
			throw runtime_error("Could not decrypt certificate data!");
		}
		// encrypt using new key
		if(!EncryptMessage(vchPubKey, vchFromString(strData), strCipherText))
		{
			throw runtime_error("Could not encrypt certificate data!");
		}
		if (strCipherText.size() > MAX_ENCRYPTED_VALUE_LENGTH)
			throw runtime_error("data length cannot exceed 1023 bytes!");
		vchData = vchFromString(strCipherText);
	}	
	CCert copyCert = theCert;
	theCert.ClearCert();
    scriptPubKeyOrig= GetScriptForDestination(xferKey.GetID());
    CScript scriptPubKey;
    scriptPubKey << CScript::EncodeOP_N(OP_CERT_TRANSFER) << vchCert << OP_2DROP;
    scriptPubKey += scriptPubKeyOrig;
	// check the offer links in the cert, can't xfer a cert thats linked to another offer
   if(!theCert.vchOfferLink.empty() && !offerpurchase)
   {
		COffer myOffer;
		CTransaction txMyOffer;
		// if offer is still valid then we cannot xfer this cert
		if (GetTxOfOffer(*pofferdb, theCert.vchOfferLink, myOffer, txMyOffer))
		{
			string strError = strprintf("Cannot transfer this certificate, it is linked to offer: %s", stringFromVch(theCert.vchOfferLink).c_str());
			throw runtime_error(strError);
		}
   }
	theCert.nHeight = chainActive.Tip()->nHeight;
	theCert.vchPubKey = vchPubKey;
	if(copyCert.vchData != vchData)
		theCert.vchData = vchData;
    // send the cert pay txn
	vector<CRecipient> vecSend;
	CRecipient recipient;
	CreateRecipient(scriptPubKey, recipient);
	vecSend.push_back(recipient);

	const vector<unsigned char> &data = theCert.Serialize();
	CScript scriptData;
	scriptData << OP_RETURN << data;
	CRecipient fee;
	CreateFeeRecipient(scriptData, data, fee);
	vecSend.push_back(fee);

	SendMoneySyscoin(vecSend, recipient.nAmount+fee.nAmount, false, wtx, wtxIn);

	UniValue res(UniValue::VARR);
	res.push_back(wtx.GetHash().GetHex());
	return res;
}


UniValue certinfo(const UniValue& params, bool fHelp) {
    if (fHelp || 1 != params.size())
        throw runtime_error("certinfo <guid>\n"
                "Show stored values of a single certificate and its .\n");

    vector<unsigned char> vchCert = vchFromValue(params[0]);

    // look for a transaction with this key, also returns
    // an cert object if it is found
    CTransaction tx;

	vector<CCert> vtxPos;

	int expired = 0;
	int expires_in = 0;
	int expired_block = 0;
	UniValue oCert(UniValue::VOBJ);
    vector<unsigned char> vchValue;

	if (!pcertdb->ReadCert(vchCert, vtxPos) || vtxPos.empty())
		throw runtime_error("failed to read from cert DB");
	CCert ca = vtxPos.back();
	uint256 blockHash;
	if (!GetTransaction(ca.txHash, tx, Params().GetConsensus(), blockHash, true))
		throw runtime_error("failed to read transaction from disk");   
    string sHeight = strprintf("%llu", ca.nHeight);
    oCert.push_back(Pair("cert", stringFromVch(vchCert)));
    oCert.push_back(Pair("txid", ca.txHash.GetHex()));
    oCert.push_back(Pair("height", sHeight));
    oCert.push_back(Pair("title", stringFromVch(ca.vchTitle)));
	string strData = stringFromVch(ca.vchData);
	string strDecrypted = "";
	if(ca.bPrivate)
	{
		if(DecryptMessage(ca.vchPubKey, ca.vchData, strDecrypted))
			strData = strDecrypted;		
	}
    oCert.push_back(Pair("data", strData));
	oCert.push_back(Pair("private", ca.bPrivate? "Yes": "No"));
    oCert.push_back(Pair("is_mine", IsCertMine(tx) ? "true" : "false"));

    uint64_t nHeight;
	nHeight = ca.nHeight;
	string strAddress = "";
    GetCertAddress(tx, strAddress);
	CSyscoinAddress address(strAddress);
	if(address.IsValid() && address.isAlias)
		oCert.push_back(Pair("address", address.aliasName));
	else
		oCert.push_back(Pair("address", address.ToString()));
	expired_block = nHeight + GetCertExpirationDepth();
	if(nHeight + GetCertExpirationDepth() - chainActive.Tip()->nHeight <= 0)
	{
		expired = 1;
	}  
	if(expired == 0)
	{
		expires_in = nHeight + GetCertExpirationDepth() - chainActive.Tip()->nHeight;
	}
	oCert.push_back(Pair("expires_in", expires_in));
	oCert.push_back(Pair("expires_on", expired_block));
	oCert.push_back(Pair("expired", expired));
    return oCert;
}

UniValue certlist(const UniValue& params, bool fHelp) {
    if (fHelp || 1 < params.size())
        throw runtime_error("certlist [<cert>]\n"
                "list my own Certificates");
	vector<unsigned char> vchName;

	if (params.size() == 1)
		vchName = vchFromValue(params[0]);
    vector<unsigned char> vchNameUniq;
    if (params.size() == 1)
        vchNameUniq = vchFromValue(params[0]);

    UniValue oRes(UniValue::VARR);
    map< vector<unsigned char>, int > vNamesI;
    map< vector<unsigned char>, UniValue > vNamesO;

    uint256 blockHash;
    uint256 hash;
    CTransaction tx;

    vector<unsigned char> vchValue;
    uint64_t nHeight;

    BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, pwalletMain->mapWallet)
    {
		const CWalletTx &wtx = item.second;
		int expired = 0;
		int expires_in = 0;
		int expired_block = 0;
		// get txn hash, read txn index
        hash = item.second.GetHash();       

        // skip non-syscoin txns
        if (wtx.nVersion != SYSCOIN_TX_VERSION)
            continue;
		// decode txn, skip non-cert txns
		vector<vector<unsigned char> > vvch;
		int op, nOut;
		if (!DecodeCertTx(wtx, op, nOut, vvch, -1) || !IsCertOp(op))
			continue;
		
		vchName = vvch[0];
		
		// skip this cert if it doesn't match the given filter value
		if (vchNameUniq.size() > 0 && vchNameUniq != vchName)
			continue;
		
		// get last active name only
		if (vNamesI.find(vchName) != vNamesI.end() && (nHeight < vNamesI[vchName] || vNamesI[vchName] < 0))
			continue;
		
		vector<CCert> vtxPos;
		if (!pcertdb->ReadCert(vchName, vtxPos) || vtxPos.empty())
			continue;
		
		CCert cert = vtxPos.back();
		if (!GetTransaction(cert.txHash, tx, Params().GetConsensus(), blockHash, true))
			continue;
		
		if(!IsCertMine(tx))
			continue;
		nHeight = cert.nHeight;
        // build the output object
		UniValue oName(UniValue::VOBJ);
        oName.push_back(Pair("cert", stringFromVch(vchName)));
        oName.push_back(Pair("title", stringFromVch(cert.vchTitle)));

		string strData = stringFromVch(cert.vchData);
		string strDecrypted = "";
		if(cert.bPrivate)
		{
			if(DecryptMessage(cert.vchPubKey, cert.vchData, strDecrypted))
                strData = strDecrypted;
		}
		oName.push_back(Pair("private", cert.bPrivate? "Yes": "No"));
		oName.push_back(Pair("data", strData));
        string strAddress = "";
        GetCertAddress(tx, strAddress);
		CSyscoinAddress address(strAddress);
		if(address.IsValid() && address.isAlias)
			oName.push_back(Pair("address", address.aliasName));
		else
			oName.push_back(Pair("address", address.ToString()));
		expired_block = nHeight + GetCertExpirationDepth();
		if(nHeight + GetCertExpirationDepth() - chainActive.Tip()->nHeight <= 0)
		{
			expired = 1;
		}  
		if(expired == 0)
		{
			expires_in = nHeight + GetCertExpirationDepth() - chainActive.Tip()->nHeight;
		}
		oName.push_back(Pair("expires_in", expires_in));
		oName.push_back(Pair("expires_on", expired_block));
		oName.push_back(Pair("expired", expired));
 
		vNamesI[vchName] = nHeight;
		vNamesO[vchName] = oName;	
    
	}
    BOOST_FOREACH(const PAIRTYPE(vector<unsigned char>, UniValue)& item, vNamesO)
        oRes.push_back(item.second);
    return oRes;
}


UniValue certhistory(const UniValue& params, bool fHelp) {
    if (fHelp || 1 != params.size())
        throw runtime_error("certhistory <cert>\n"
                "List all stored values of an cert.\n");

    UniValue oRes(UniValue::VARR);
    vector<unsigned char> vchCert = vchFromValue(params[0]);
    string cert = stringFromVch(vchCert);

    {
        vector<CCert> vtxPos;
        if (!pcertdb->ReadCert(vchCert, vtxPos) || vtxPos.empty())
            throw runtime_error("failed to read from cert DB");

        CCert txPos2;
        uint256 txHash;
        uint256 blockHash;
        BOOST_FOREACH(txPos2, vtxPos) {
            txHash = txPos2.txHash;
			CTransaction tx;
			if (!GetTransaction(txHash, tx, Params().GetConsensus(), blockHash, true)) {
				error("could not read txpos");
				continue;
			}

			int expired = 0;
			int expires_in = 0;
			int expired_block = 0;
            UniValue oCert(UniValue::VOBJ);
            vector<unsigned char> vchValue;
            uint64_t nHeight;
			nHeight = txPos2.nHeight;
            oCert.push_back(Pair("cert", cert));
            string value = stringFromVch(vchValue);
            oCert.push_back(Pair("value", value));
            oCert.push_back(Pair("txid", tx.GetHash().GetHex()));
            string strAddress = "";
            GetCertAddress(tx, strAddress);
			CSyscoinAddress address(strAddress);
			if(address.IsValid() && address.isAlias)
				oCert.push_back(Pair("address", address.aliasName));
			else
				oCert.push_back(Pair("address", address.ToString()));
			expired_block = nHeight + GetCertExpirationDepth();
			if(nHeight + GetCertExpirationDepth() - chainActive.Tip()->nHeight <= 0)
			{
				expired = 1;
			}  
			if(expired == 0)
			{
				expires_in = nHeight + GetCertExpirationDepth() - chainActive.Tip()->nHeight;
			}
			oCert.push_back(Pair("expires_in", expires_in));
			oCert.push_back(Pair("expires_on", expired_block));
			oCert.push_back(Pair("expired", expired));
            oRes.push_back(oCert);
        }
    }
    return oRes;
}

UniValue certfilter(const UniValue& params, bool fHelp) {
    if (fHelp || params.size() > 5)
        throw runtime_error(
                "certfilter [[[[[regexp] maxage=36000] from=0] nb=0] stat]\n"
                        "scan and filter certes\n"
                        "[regexp] : apply [regexp] on certes, empty means all certes\n"
                        "[maxage] : look in last [maxage] blocks\n"
                        "[from] : show results from number [from]\n"
                        "[nb] : show [nb] results, 0 means all\n"
                        "[stats] : show some stats instead of results\n"
                        "certfilter \"\" 5 # list certes updated in last 5 blocks\n"
                        "certfilter \"^cert\" # list all certes starting with \"cert\"\n"
                        "certfilter 36000 0 0 stat # display stats (number of certs) on active certes\n");

    string strRegexp;
    int nFrom = 0;
    int nNb = 0;
    int nMaxAge = GetCertExpirationDepth();
    bool fStat = false;
    int nCountFrom = 0;
    int nCountNb = 0;

    if (params.size() > 0)
        strRegexp = params[0].get_str();

    if (params.size() > 1)
        nMaxAge = params[1].get_int();

    if (params.size() > 2)
        nFrom = params[2].get_int();

    if (params.size() > 3)
        nNb = params[3].get_int();

    if (params.size() > 4)
        fStat = (params[4].get_str() == "stat" ? true : false);

    //CCertDB dbCert("r");
    UniValue oRes(UniValue::VARR);

    vector<unsigned char> vchCert;
    vector<pair<vector<unsigned char>, CCert> > certScan;
    if (!pcertdb->ScanCerts(vchCert, 100000000, certScan))
        throw runtime_error("scan failed");
    // regexp
    using namespace boost::xpressive;
    smatch certparts;
    sregex cregex = sregex::compile(strRegexp);
    pair<vector<unsigned char>, CCert> pairScan;
	BOOST_FOREACH(pairScan, certScan) {
		CCert txCert = pairScan.second;
		string cert = stringFromVch(pairScan.first);
		string title = stringFromVch(txCert.vchTitle);
        if (strRegexp != "" && !regex_search(title, certparts, cregex) && strRegexp != cert)
            continue;

        
        int nHeight = txCert.nHeight;

        // max age
        if (nMaxAge != 0 && chainActive.Tip()->nHeight - nHeight >= nMaxAge)
            continue;
        // from limits
        nCountFrom++;
        if (nCountFrom < nFrom + 1)
            continue;
        CTransaction tx;
        uint256 blockHash;
		if (!GetTransaction(txCert.txHash, tx, Params().GetConsensus(), blockHash, true))
			continue;

		int expired = 0;
		int expires_in = 0;
		int expired_block = 0;
        UniValue oCert(UniValue::VOBJ);
        oCert.push_back(Pair("cert", cert));
		vector<unsigned char> vchValue = txCert.vchTitle;
        string value = stringFromVch(vchValue);
        oCert.push_back(Pair("title", value));

		string strData = stringFromVch(txCert.vchData);
		string strDecrypted = "";
		if(txCert.bPrivate)
		{
			strData = string("Encrypted for owner of certificate");
			if(DecryptMessage(txCert.vchPubKey, txCert.vchData, strDecrypted))
				strData = strDecrypted;
			
		}

		oCert.push_back(Pair("data", strData));
		oCert.push_back(Pair("private", txCert.bPrivate? "Yes": "No"));
		expired_block = nHeight + GetCertExpirationDepth();
		if(nHeight + GetCertExpirationDepth() - chainActive.Tip()->nHeight <= 0)
		{
			expired = 1;
		}  
		if(expired == 0)
		{
			expires_in = nHeight + GetCertExpirationDepth() - chainActive.Tip()->nHeight;
		}
		oCert.push_back(Pair("expires_in", expires_in));
		oCert.push_back(Pair("expires_on", expired_block));
		oCert.push_back(Pair("expired", expired));

        oRes.push_back(oCert);

        nCountNb++;
        // nb limits
        if (nNb > 0 && nCountNb >= nNb)
            break;
    }

    if (fStat) {
        UniValue oStat(UniValue::VOBJ);
        oStat.push_back(Pair("blocks", (int) chainActive.Tip()->nHeight));
        oStat.push_back(Pair("count", (int) oRes.size()));
        //oStat.push_back(Pair("sha256sum", SHA256(oRes), true));
        return oStat;
    }

    return oRes;
}

UniValue certscan(const UniValue& params, bool fHelp) {
    if (fHelp || 2 > params.size())
        throw runtime_error(
                "certscan [<start-cert>] [<max-returned>]\n"
                        "scan all certs, starting at start-cert and returning a maximum number of entries (default 500)\n");

    vector<unsigned char> vchCert;
    int nMax = 500;
    if (params.size() > 0) {
        vchCert = vchFromValue(params[0]);
    }

    if (params.size() > 1) {
        nMax = params[1].get_int();
    }

    //CCertDB dbCert("r");
    UniValue oRes(UniValue::VARR);

    vector<pair<vector<unsigned char>, CCert> > certScan;
    if (!pcertdb->ScanCerts(vchCert, nMax, certScan))
        throw runtime_error("scan failed");

    pair<vector<unsigned char>, CCert> pairScan;
    BOOST_FOREACH(pairScan, certScan) {
        UniValue oCert(UniValue::VOBJ);
        string cert = stringFromVch(pairScan.first);
        oCert.push_back(Pair("cert", cert));
        CTransaction tx;
        CCert txCert = pairScan.second;
        uint256 blockHash;
		int expired = 0;
		int expires_in = 0;
		int expired_block = 0;
        int nHeight = txCert.nHeight;
        vector<unsigned char> vchValue = txCert.vchTitle;
        string value = stringFromVch(vchValue);
		if (!GetTransaction(txCert.txHash, tx, Params().GetConsensus(), blockHash, true))
			continue;

        //string strAddress = "";
        //GetCertAddress(tx, strAddress);
        oCert.push_back(Pair("value", value));
        //oCert.push_back(Pair("txid", tx.GetHash().GetHex()));
        //oCert.push_back(Pair("address", strAddress));
		expired_block = nHeight + GetCertExpirationDepth();
		if(nHeight + GetCertExpirationDepth() - chainActive.Tip()->nHeight <= 0)
		{
			expired = 1;
		}  
		if(expired == 0)
		{
			expires_in = nHeight + GetCertExpirationDepth() - chainActive.Tip()->nHeight;
		}
		oCert.push_back(Pair("expires_in", expires_in));
		oCert.push_back(Pair("expires_on", expired_block));
		oCert.push_back(Pair("expired", expired));
			
		oRes.push_back(oCert);
    }

    return oRes;
}



