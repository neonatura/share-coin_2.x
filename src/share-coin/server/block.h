
/*
 * @copyright
 *
 *  Copyright 2014 Neo Natura
 *
 *  This file is part of the Share Library.
 *  (https://github.com/neonatura/share)
 *        
 *  The Share Library is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version. 
 *
 *  The Share Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with The Share Library.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  @endcopyright
 */  

#ifndef __SERVER__BLOCK_H__
#define __SERVER__BLOCK_H__

//#include "shcoind.h"
#include <boost/foreach.hpp>
#include <vector>

#include "uint256.h"
#include "serialize.h"
#include "util.h"
#include "scrypt.h"
#include "protocol.h"
#include "net.h"
#include "script.h"
#include "coin_proto.h"
#include "txext.h"
#include "matrix.h"

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
using namespace std;
using namespace json_spirit;


typedef std::vector<uint256> HashList;


class CTxDB;


bc_t *GetBlockChain(CIface *iface);

bc_t *GetBlockTxChain(CIface *iface);;


void CloseBlockChains(void);



bool GetTransaction(CIface *iface, const uint256 &hash, CTransaction &tx, uint256 *hashBlock);



/* block_iface.cpp */
int GetBlockDepthInMainChain(CIface *iface, uint256 blockHash);
int GetTxDepthInMainChain(CIface *iface, uint256 txHash);




extern FILE* AppendBlockFile(unsigned int& nFileRet);
extern bool IsInitialBlockDownload();
FILE* OpenBlockFile(unsigned int nFile, unsigned int nBlockPos, const char* pszMode);



enum GetMinFee_mode
{
    GMF_BLOCK,
    GMF_RELAY,
    GMF_SEND,
};

static const unsigned int LOCKTIME_THRESHOLD = 500000000; // Tue Nov  5 00:53:20 1985 UTC


#if 0
/* 1MEG Max Block Size */
#define MAX_BLOCK_SIZE USDE_MAX_BLOCK_SIZE
#define MAX_BLOCK_SIGOPS USDE_MAX_BLOCK_SIGOPS
#define MIN_TX_FEE USDE_MIN_TX_FEE
#define MIN_RELAY_TX_FEE USDE_MIN_RELAY_TX_FEE
#define MAX_MONEY USDE_MAX_MONEY
#define COINBASE_MATURITY USDE_COINBASE_MATURITY
#endif

#if 0
static const unsigned int MAX_BLOCK_SIZE = 1000000;
static const unsigned int MAX_BLOCK_SIZE_GEN = MAX_BLOCK_SIZE/2;
static const unsigned int MAX_BLOCK_SIGOPS = MAX_BLOCK_SIZE/50;
static const unsigned int MAX_ORPHAN_TRANSACTIONS = MAX_BLOCK_SIZE/100;
static const int64 MIN_TX_FEE = 10000000;
static const int64 MIN_RELAY_TX_FEE = MIN_TX_FEE;
#if CLIENT_VERSION_REVISION > 4
static const int64 MAX_MONEY = 320000000000 * COIN; /* 320bil */
#else
static const int64 MAX_MONEY = 1600000000 * COIN; /* 1.6bil */
#endif
static const int COINBASE_MATURITY = 100;
#endif


inline bool MoneyRange(CIface *iface, int64 nValue) 
{ 
  if (!iface) return (false);
  return (nValue >= 0 && nValue <= iface->max_money);
}
inline bool MoneyRange(int ifaceIndex, int64 nValue) 
{
  CIface *iface = GetCoinByIndex(ifaceIndex);
  if (!iface) return (false);
  return (nValue >= 0 && nValue <= iface->max_money);
}


/** Reference to a specific block transaction. */
class CDiskTxPos
{
public:
    unsigned int nFile;
    unsigned int nBlockPos;
    unsigned int nTxPos;
    mutable uint256 hashBlock;
    mutable uint256 hashTx;

    CDiskTxPos()
    {
        SetNull();
    }

    CDiskTxPos(unsigned int nFileIn, unsigned int nBlockPosIn, unsigned int nTxPosIn)
    {
      SetNull();
        nFile = nFileIn;
        nBlockPos = nBlockPosIn;
        nTxPos = nTxPosIn;
    }
#if 0
    CDiskTxPos(uint256 hash, uint256 tx_hash)
    {
      SetNull();
hashBlock = hash;
hashTx = tx_hash;
    }
#endif

    IMPLEMENT_SERIALIZE( READWRITE(FLATDATA(*this)); )
    void SetNull() { nFile = (unsigned int) -1; nBlockPos = 0; nTxPos = 0; }
    bool IsNull() const { return (nFile == (unsigned int) -1); }

    friend bool operator==(const CDiskTxPos& a, const CDiskTxPos& b)
    {
        return (a.nFile     == b.nFile &&
                a.nBlockPos == b.nBlockPos &&
                a.nTxPos    == b.nTxPos);
#if 0
        return (a.hashBlock == b.hashBlock &&
                a.hashTx    == b.hashTx);
#endif
    }

    friend bool operator!=(const CDiskTxPos& a, const CDiskTxPos& b)
    {
        return !(a == b);
    }

    std::string ToString() const
    {
        if (IsNull())
            return "null";
        else
            return strprintf("(nTxHeight=%d, nBlockHeight=%d, nTxPos=%d)", nFile, nBlockPos, nTxPos);
    }

    void print() const
    {
        printf("%s", ToString().c_str());
    }

};


/**  A txdb record that contains the disk location of a transaction and the
 * locations of transactions that spend its outputs.  vSpent is really only
 * used as a flag, but having the location is very helpful for debugging.
 */
class CTxIndex
{
public:
    CDiskTxPos pos;
    std::vector<CDiskTxPos> vSpent;

    CTxIndex()
    {
        SetNull();
    }

    CTxIndex(const CDiskTxPos& posIn, unsigned int nOutputs)
    {
        pos = posIn;
        vSpent.resize(nOutputs);
    }

    IMPLEMENT_SERIALIZE
    (
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(pos);
        READWRITE(vSpent);
    )

    void SetNull()
    {
        pos.SetNull();
        vSpent.clear();
    }

    bool IsNull()
    {
        return pos.IsNull();
    }

    friend bool operator==(const CTxIndex& a, const CTxIndex& b)
    {
        return (a.pos    == b.pos &&
                a.vSpent == b.vSpent);
    }

    friend bool operator!=(const CTxIndex& a, const CTxIndex& b)
    {
        return !(a == b);
    }
//    int GetDepthInMainChain() const;
 
};
typedef std::map<uint256, std::pair<CTxIndex, CTransaction> > MapPrevTx;



/** An inpoint - a combination of a transaction and an index n into its vin */
class CInPoint
{
public:
    CTransaction* ptx;
    unsigned int n;

    CInPoint() { SetNull(); }
    CInPoint(CTransaction* ptxIn, unsigned int nIn) { ptx = ptxIn; n = nIn; }
    void SetNull() { ptx = NULL; n = (unsigned int) -1; }
    bool IsNull() const { return (ptx == NULL && n == (unsigned int) -1); }
};



/** An outpoint - a combination of a transaction hash and an index n into its vout */
class COutPoint
{
public:
    uint256 hash;
    unsigned int n;

    COutPoint() { SetNull(); }
    COutPoint(uint256 hashIn, unsigned int nIn) { hash = hashIn; n = nIn; }
    IMPLEMENT_SERIALIZE( READWRITE(FLATDATA(*this)); )
    void SetNull() { hash = 0; n = (unsigned int) -1; }
    bool IsNull() const { return (hash == 0 && n == (unsigned int) -1); }

    friend bool operator<(const COutPoint& a, const COutPoint& b)
    {
        return (a.hash < b.hash || (a.hash == b.hash && a.n < b.n));
    }

    friend bool operator==(const COutPoint& a, const COutPoint& b)
    {
        return (a.hash == b.hash && a.n == b.n);
    }

    friend bool operator!=(const COutPoint& a, const COutPoint& b)
    {
        return !(a == b);
    }

    std::string ToString() const
    {
        return strprintf("COutPoint(%s, %d)", hash.ToString().substr(0,10).c_str(), n);
    }

    void print() const
    {
        printf("%s\n", ToString().c_str());
    }
};



/** An input of a transaction.  It contains the location of the previous
 * transaction's output that it claims and a signature that matches the
 * output's public key.
 */
class CTxIn
{
public:
    COutPoint prevout;
    CScript scriptSig;
    unsigned int nSequence;

    CTxIn()
    {
        nSequence = std::numeric_limits<unsigned int>::max();
    }

    explicit CTxIn(COutPoint prevoutIn, CScript scriptSigIn=CScript(), unsigned int nSequenceIn=std::numeric_limits<unsigned int>::max())
    {
        prevout = prevoutIn;
        scriptSig = scriptSigIn;
        nSequence = nSequenceIn;
    }

    CTxIn(uint256 hashPrevTx, unsigned int nOut, CScript scriptSigIn=CScript(), unsigned int nSequenceIn=std::numeric_limits<unsigned int>::max())
    {
        prevout = COutPoint(hashPrevTx, nOut);
        scriptSig = scriptSigIn;
        nSequence = nSequenceIn;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(prevout);
        READWRITE(scriptSig);
        READWRITE(nSequence);
    )

    bool IsFinal() const
    {
        return (nSequence == std::numeric_limits<unsigned int>::max());
    }

    friend bool operator==(const CTxIn& a, const CTxIn& b)
    {
        return (a.prevout   == b.prevout &&
                a.scriptSig == b.scriptSig &&
                a.nSequence == b.nSequence);
    }

    friend bool operator!=(const CTxIn& a, const CTxIn& b)
    {
        return !(a == b);
    }

    std::string ToString() const
    {
        std::string str;
        str += "CTxIn(";
        str += prevout.ToString();
        if (prevout.IsNull())
            str += strprintf(", coinbase %s", HexStr(scriptSig).c_str());
        else
            str += strprintf(", scriptSig=%s", scriptSig.ToString().substr(0,24).c_str());
        if (nSequence != std::numeric_limits<unsigned int>::max())
            str += strprintf(", nSequence=%u", nSequence);
        str += ")";
        return str;
    }

    void print() const
    {
        printf("%s\n", ToString().c_str());
    }
};




/** An output of a transaction.  It contains the public key that the next input
 * must be able to sign with to claim it.
 */
class CTxOut
{
public:
    int64 nValue;
    CScript scriptPubKey;

    CTxOut()
    {
        SetNull();
    }

    CTxOut(int64 nValueIn, CScript scriptPubKeyIn)
    {
        nValue = nValueIn;
        scriptPubKey = scriptPubKeyIn;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(nValue);
        READWRITE(scriptPubKey);
    )

    void SetNull()
    {
        nValue = -1;
        scriptPubKey.clear();
    }

    bool IsNull()
    {
        return (nValue == -1);
    }

    uint256 GetHash() const
    {
      return SerializeHash(*this);
    }

    friend bool operator==(const CTxOut& a, const CTxOut& b)
    {
        return (a.nValue       == b.nValue &&
                a.scriptPubKey == b.scriptPubKey);
    }

    friend bool operator!=(const CTxOut& a, const CTxOut& b)
    {
        return !(a == b);
    }

    std::string ToString() const
    {
        if (scriptPubKey.size() < 6)
            return "CTxOut(error)";
        return strprintf("CTxOut(nValue=%"PRI64d".%08"PRI64d", scriptPubKey=%s)", nValue / COIN, nValue % COIN, scriptPubKey.ToString().substr(0,30).c_str());
    }

    void print() const
    {
        printf("%s\n", ToString().c_str());
    }
};



class CBlock;
class CBlockIndex;
typedef std::map<uint256, CBlockIndex*> blkidx_t;

#define TX_VERSION TXF_VERSION
#define TX_VERSION_2 TXF_VERSION_2

class CTransactionCore
{
  public:

    static const int TXF_VERSION = (1 << 0);
    static const int TXF_VERSION_2 = (1 << 1);
    static const int TXF_CERTIFICATE = (1 << 4);
    static const int TXF_LICENSE = (1 << 5);
    static const int TXF_ALIAS = (1 << 6);
    static const int TXF_OFFER = (1 << 7);
    static const int TXF_OFFER_ACCEPT = (1 << 8);
    static const int TXF_ASSET = (1 << 9);
    static const int TXF_IDENT = (1 << 10);
    static const int TXF_MATRIX = (1 << 11);

    int nFlag;
    std::vector<CTxIn> vin;
    std::vector<CTxOut> vout;
    unsigned int nLockTime;

    CTransactionCore()
    {
        SetNull();
    }

    IMPLEMENT_SERIALIZE
    (
        //READWRITE(this->nVersion);
        READWRITE(this->nFlag);
        nVersion = 1;//this->nVersion;
        READWRITE(vin);
        READWRITE(vout);
        READWRITE(nLockTime);
    )

    void SetNull()
    {
        nFlag = CTransactionCore::TXF_VERSION;
        vin.clear();
        vout.clear();
        nLockTime = 0;
    }

    void Init(CTransactionCore tx)
    {
      nFlag = tx.nFlag;
      vin = tx.vin;
      vout = tx.vout;
      nLockTime = tx.nLockTime;
    }

    bool IsNull() const
    {
        return (vin.empty() && vout.empty());
    }

    bool isFlag(int flag) const
    {
      if ( (nFlag & flag) ) {
        return (true);
      } 
      return (false);
    }

    std::string ToString();

    Object ToValue();

};


/** The basic transaction that is broadcasted on the network and contained in
 * blocks.  A transaction can contain multiple inputs and outputs.
 */
class CTransaction : public CTransactionCore
{
public:

    CCert certificate;
    CAlias alias;
    COffer offer;
    CTxMatrix matrix;

    CTransaction()
    {
      SetNull();
    }
    CTransaction(const CTransaction& tx)
    {
      SetNull();
      Init(tx);
    }

    IMPLEMENT_SERIALIZE
    (
      READWRITE(*(CTransactionCore*)this);
      if ((this->nFlag & TXF_CERTIFICATE) ||
          (this->nFlag & TXF_LICENSE) ||
          (this->nFlag & TXF_ASSET) ||
          (this->nFlag & TXF_IDENT))
        READWRITE(certificate);
      if (this->nFlag & TXF_ALIAS)
        READWRITE(alias);
      if ((this->nFlag & TXF_OFFER) ||
          (this->nFlag & TXF_OFFER_ACCEPT))
        READWRITE(offer);
    )

    void Init(const CTransaction& tx)
    {
      CTransactionCore::Init(tx);
      if ((this->nFlag & TXF_IDENT) ||
          (this->nFlag & TXF_CERTIFICATE) ||
          (this->nFlag & TXF_LICENSE) ||
          (this->nFlag & TXF_ASSET))
        certificate = tx.certificate;
      if (this->nFlag & TXF_ALIAS)
        alias = tx.alias;
      if ((this->nFlag & TXF_OFFER) ||
          (this->nFlag & TXF_OFFER_ACCEPT))
        offer = tx.offer;
#if 0
      if (this->nFlag & TXF_IDENT)
        ident = tx.ident;
#endif
    }

    void SetNull()
    {
      CTransactionCore::SetNull();
    }

    uint256 GetHash() const
    {
      return SerializeHash(*this);
    }

    bool IsFinal(int ifaceIndex, int nBlockHeight=0, int64 nBlockTime=0) const;

    bool IsNewerThan(const CTransaction& old) const
    {
        if (vin.size() != old.vin.size())
            return false;
        for (unsigned int i = 0; i < vin.size(); i++)
            if (vin[i].prevout != old.vin[i].prevout)
                return false;

        bool fNewer = false;
        unsigned int nLowest = std::numeric_limits<unsigned int>::max();
        for (unsigned int i = 0; i < vin.size(); i++)
        {
            if (vin[i].nSequence != old.vin[i].nSequence)
            {
                if (vin[i].nSequence <= nLowest)
                {
                    fNewer = false;
                    nLowest = vin[i].nSequence;
                }
                if (old.vin[i].nSequence < nLowest)
                {
                    fNewer = true;
                    nLowest = old.vin[i].nSequence;
                }
            }
        }
        return fNewer;
    }

    bool IsCoinBase() const
    {
        return (vin.size() == 1 && vin[0].prevout.IsNull());
    }

    /** Check for standard transaction types
        @return True if all outputs (scriptPubKeys) use only standard transaction forms
    */
    bool IsStandard() const;

    /** Check for standard transaction types
        @param[in] mapInputs	Map of previous transactions that have outputs we're spending
        @return True if all inputs (scriptSigs) use only standard transaction forms
        @see CTransaction::FetchInputs
    */
    bool AreInputsStandard(const MapPrevTx& mapInputs) const;

    /** Count ECDSA signature operations the old-fashioned (pre-0.6) way
        @return number of sigops this transaction's outputs will produce when spent
        @see CTransaction::FetchInputs
    */
    unsigned int GetLegacySigOpCount() const;

    /** Count ECDSA signature operations in pay-to-script-hash inputs.

        @param[in] mapInputs	Map of previous transactions that have outputs we're spending
        @return maximum number of sigops required to validate this transaction's inputs
        @see CTransaction::FetchInputs
     */
    unsigned int GetP2SHSigOpCount(const MapPrevTx& mapInputs) const;

    /** Amount of bitcoins spent by this transaction.
        @return sum of all outputs (note: does not include fees)
     */
    int64 GetValueOut() const
    {
        int64 nValueOut = 0;
        BOOST_FOREACH(const CTxOut& txout, vout)
        {
            nValueOut += txout.nValue;
#if 0
            if (!MoneyRange(iface, txout.nValue) || !MoneyRange(iface, nValueOut))
                throw std::runtime_error("CTransaction::GetValueOut() : value out of range");
#endif
        }
        return nValueOut;
    }

    /** Amount of bitcoins coming in to this transaction
        Note that lightweight clients may not know anything besides the hash of previous transactions,
        so may not be able to calculate this.

        @param[in] mapInputs	Map of previous transactions that have outputs we're spending
        @return	Sum of value of all inputs (scriptSigs)
        @see CTransaction::FetchInputs
     */
    int64 GetValueIn(const MapPrevTx& mapInputs) const;

    static bool AllowFree(double dPriority)
    {
        // Large (in bytes) low-priority (new, small-coin) transactions
        // need a fee.
        return dPriority > COIN * 700 / 250; // usde: 480 blocks found a day. Priority cutoff is 1 usde day / 250 bytes.
    }

    int64 GetMinFee(int ifaceIndex, unsigned int nBlockSize=1, bool fAllowFree=true, enum GetMinFee_mode mode=GMF_BLOCK) const
    {
      CIface *iface = GetCoinByIndex(ifaceIndex);
      if (!iface)
        return (0);

      // Base fee is either MIN_TX_FEE or MIN_RELAY_TX_FEE
      //int64 nBaseFee = (mode == GMF_RELAY) ? MIN_RELAY_TX_FEE : MIN_TX_FEE;
      int64 nBaseFee = (mode == GMF_RELAY) ? iface->min_relay_tx_fee : iface->min_tx_fee;

      unsigned int nBytes = ::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION(iface));
      unsigned int nNewBlockSize = nBlockSize + nBytes;
      int64 nMinFee = (1 + (int64)nBytes / 1000) * nBaseFee;

      if (fAllowFree)
      {
        if (nBlockSize == 1)
        {
          // Transactions under 10K are free
          // (about 4500bc if made of 50bc inputs)
          if (nBytes < 10000)
            nMinFee = 0;
        }
        else
        {
          // Free transaction area
          if (nNewBlockSize < 27000)
            nMinFee = 0;
        }
      }

      // To limit dust spam, add MIN_TX_FEE/MIN_RELAY_TX_FEE for any output that is less than 0.01
      BOOST_FOREACH(const CTxOut& txout, vout)
        if (txout.nValue < CENT)
          nMinFee += nBaseFee;

      // Raise the price as the block approaches full
      if (nBlockSize != 1 && nNewBlockSize >= MAX_BLOCK_SIZE_GEN(iface)/2)
      {
        if (nNewBlockSize >= MAX_BLOCK_SIZE_GEN(iface))
          return (iface->max_money);
        nMinFee *= MAX_BLOCK_SIZE_GEN(iface) / (MAX_BLOCK_SIZE_GEN(iface) - nNewBlockSize);
      }

      if (!MoneyRange(iface, nMinFee))
        nMinFee = iface->max_money;
      return nMinFee;
    }

#if 0
    bool ReadFromDisk(CDiskTxPos pos, FILE** pfileRet=NULL)
    {
        CAutoFile filein = CAutoFile(OpenBlockFile(pos.nFile, 0, pfileRet ? "rb+" : "rb"), SER_DISK, CLIENT_VERSION);
        if (!filein)
            return error(SHERR_IO, "CTransaction::ReadFromDisk() : OpenBlockFile failed");

        // Read transaction
        if (fseek(filein, pos.nTxPos, SEEK_SET) != 0)
            return error(SHERR_IO, "CTransaction::ReadFromDisk() : fseek failed");

        try {
            filein >> *this;
        }
        catch (std::exception &e) {
            return error(SHERR_IO, "%s() : deserialize or I/O error", __PRETTY_FUNCTION__);
        }

        // Return file pointer
        if (pfileRet)
        {
            if (fseek(filein, pos.nTxPos, SEEK_SET) != 0)
                return error(SHERR_IO, "CTransaction::ReadFromDisk() : second fseek failed");
            *pfileRet = filein.release();
        }
        return true;
    }
#endif

#if 0
    bool ReadFromDisk(int ifaceIndex, CDiskTxPos pos)
    {
      ReadTx(ifaceIndex, pos.hashTx);
    }
#endif

    bool ReadTx(int ifaceIndex, uint256 txHash);

    bool ReadTx(int ifaceIndex, uint256 txHash, uint256 *hashBlock);

    bool WriteTx(int ifaceIndex, uint64_t blockHeight);

    bool ReadFromDisk(CDiskTxPos pos);
    bool ReadFromDisk(int ifaceIndex, COutPoint prevout);

    bool ReadFromDisk(CTxDB& txdb, COutPoint prevout, CTxIndex& txindexRet);

    bool FillTx(int ifaceIndex, CDiskTxPos &pos);

    bool EraseTx(int ifaceIndex);


    CAlias *CreateAlias(std::string name, const uint160& hash);

    CCert *CreateCert(int ifaceIndex, const char *name, CCoinAddr& addr, cbuff secret, int64 nLicenseFee);
    CCert *CreateLicense(CCert *cert);

    COffer *CreateOffer();
    COfferAccept *AcceptOffer(COffer *offerIn);
    COffer *GenerateOffer(COffer *offerIn);
    COfferAccept *PayOffer(COfferAccept *accept);
    COffer *RemoveOffer(uint160 hashOffer);

    CCert *CreateAsset(string strAssetName, string strAssetHash);
    CCert *UpdateAsset(const CAsset& assetIn, string strAssetName, string strAssetHash);
    CCert *SignAsset(const CAsset& assetIn, uint160 hashCert);
    CCert *RemoveAsset(const CAsset& assetIn);

    CIdent *CreateIdent(CIdent *ident);

    CIdent *CreateIdent(int ifaceIndex, CCoinAddr& addr);


    CAlias *GetAlias()
    {
      return (&alias);
    }

    CTxMatrix *GenerateValidateMatrix(int ifaceIndex, CTxMatrix *seed, CBlockIndex *pindex = NULL);

    bool VerifyValidateMatrix(CTxMatrix *seed, const CTxMatrix& matrix, CBlockIndex *pindex);

    CTxMatrix *GenerateSpringMatrix(int ifaceIndex, CIdent& ident);

    bool VerifySpringMatrix(int ifaceIndex, const CTxMatrix& matrix, shnum_t *lat_p, shnum_t *lon_p);

    CTxMatrix *GetMatrix()
    {
      if (!isFlag(TXF_MATRIX))
        return (NULL);
      return (&matrix);
    }

    /**
     * Verifies whether a vSpent has been spent.
     * @param hashTx The hash of the transaction attempting to spend the input.
     */
    bool IsSpentTx(const CDiskTxPos& pos)
    {
      uint256 hashTx = GetHash();

      if (pos.IsNull())
        return (false);

      /* this coin has been marked as spent. ensure this is not a re-write of the same transaction. */
      CTransaction spent;
      if (!spent.ReadFromDisk(pos))
        return false; /* spent being referenced does not exist. */

      if (hashTx == spent.GetHash())
        return false; /* spent was already cataloged in past */

      return true;
    }

    friend bool operator==(const CTransaction& a, const CTransaction& b)
    {
        return (a.nFlag  == b.nFlag &&
                a.vin       == b.vin &&
                a.vout      == b.vout &&
                a.nLockTime == b.nLockTime);
    }

    friend bool operator!=(const CTransaction& a, const CTransaction& b)
    {
        return !(a == b);
    }

#if 0
    std::string ToString() const
    {
        std::string str;
        str += strprintf("CTransaction(hash=%s, flag=%d, vin.size=%d, vout.size=%d, nLockTime=%d)\n",
            GetHash().ToString().substr(0,10).c_str(),
            nFlag,
            vin.size(),
            vout.size(),
            nLockTime);
        for (unsigned int i = 0; i < vin.size(); i++)
            str += "    " + vin[i].ToString() + "\n";
        for (unsigned int i = 0; i < vout.size(); i++)
            str += "    " + vout[i].ToString() + "\n";
        return str;
    }
#endif

    void print()
    {
      shcoind_log(ToString().c_str());
    }


    bool DisconnectInputs(CTxDB& txdb);

    /** 
     * Fetch from memory and/or disk. inputsRet keys are transaction hashes.
     *
     * @param[in] txdb  Transaction database
     * @param[in] mapTestPool List of pending changes to the transaction index database
     * @param[in] fBlock  True if being called to add a new best-block to the chain
     * @param[in] fMiner  True if being called by CreateNewBlock
     * @param[out] inputsRet  Pointers to this transaction's inputs
     * @param[out] fInvalid returns true if transaction is invalid
     * @return  Returns true if all inputs are in txdb or mapTestPool
     */
    bool FetchInputs(CTxDB& txdb, const std::map<uint256, CTxIndex>& mapTestPool, CBlock *pblockNew, bool fMiner, MapPrevTx& inputsRet, bool& fInvalid);

    bool ClientConnectInputs(int ifaceIndex);

    bool CheckTransaction(int ifaceIndex); 

    bool CheckTransactionInputs(int ifaceIndex);

    bool AcceptToMemoryPool(CTxDB& txdb, bool fCheckInputs=true, bool* pfMissingInputs=NULL);

    bool IsInMemoryPool(int ifaceIndex);

    Object ToValue();

    Object ToValue(CBlock *pblock);

    std::string ToString();


protected:
    const CTxOut& GetOutputFor(const CTxIn& input, const MapPrevTx& inputs) const;
};

class CBlockHeader
{
public:
    /* block header */
    int nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    unsigned int nTime;
    unsigned int nBits;
    unsigned int nNonce;

    mutable int ifaceIndex;

    CBlockHeader()
    {
      nVersion = 1;
      SetNull();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);
    )

    void SetNull()
    {
      hashPrevBlock = 0;
      hashMerkleRoot = 0;
      nTime = 0;
      nBits = 0;
      nNonce = 0;
    }

    bool IsNull() const
    {
        return (nBits == 0);
    }

    uint256 GetHash() const
    {
        return Hash(BEGIN(nVersion), END(nNonce));
    }

    int64 GetBlockTime() const
    {
        return (int64)nTime;
    }

    Object ToValue();

    std::string ToString();

};


/** Nodes collect new transactions into a block, hash them into a hash tree,
 * and scan through nonce values to make the block's hash satisfy proof-of-work
 * requirements.  When they solve the proof-of-work, they broadcast the block
 * to everyone and the block is added to the block chain.  The first transaction
 * in the block is a special one that creates a new coin owned by the creator
 * of the block.
 */
class CBlock : public CBlockHeader
{
  public:
    std::vector<CTransaction> vtx;
    mutable std::vector<uint256> vMerkleTree; /* mem only */
    mutable CNode *originPeer;

    CBlock()
    {
      SetNull();
    }

    CBlock(const CBlockHeader &header)
    {
      SetNull();
      *((CBlockHeader*)this) = header;
    }

    IMPLEMENT_SERIALIZE
      (
       READWRITE(*(CBlockHeader*)this);
       READWRITE(vtx);
      )

    void SetNull()
    {
      CBlockHeader::SetNull();
      vtx.clear();
      vMerkleTree.clear();
    }

    bool IsNull() const
    {
      return (nBits == 0);
    }

    /**
     * Obtain the block hash used to identify it's "difficulty".
     * @see CBlockHeader.nBits
     */
    uint256 GetPoWHash() const
    {
      uint256 thash;
      scrypt_1024_1_1_256(BEGIN(nVersion), BEGIN(thash));
      return thash;
    }

    /**
     * Generate the merkle root hash from all the block's transaction hashes.
     * @see CBlockHeader.nMerkleRoot
     */
    uint256 BuildMerkleTree() const;

    std::vector<uint256> GetMerkleBranch(int nIndex) const;

    /**
     * Verify a block's merkle root hash.
     */
    uint256 CheckMerkleBranch(uint256 hash, const std::vector<uint256>& vMerkleBranch, int nIndex);

    void UpdateTime(const CBlockIndex* pindexPrev);

    /**
     * Permanently store a block's contents to disk.
     * @param The height to associate with the stored block content.
     */
    bool WriteBlock(uint64_t nHeight);

    bool WriteArchBlock();

    bool ReadFromDisk(const CBlockIndex* pindex, bool fReadTransactions=true);

    /**
     * Obtain a transaction in the block.
     * @param The transaction hash to obtain.
     */
    const CTransaction *GetTx(uint256 hash);

    /**
     * Track praised and dubious behaviour of a remote coin service.
     */
    bool trust(int deg, const char *msg, ...);

    /**
     * Verify the neccessary transactions exist that are required in order to perform the underlying transaction.
     */
    bool CheckTransactionInputs(int ifaceIndex);

    /**
     * Obtain the header portion of the block content.
     */ 
    CBlockHeader GetBlockHeader() const
    {
      CBlockHeader block;
      block.nVersion       = nVersion;
      block.hashPrevBlock  = hashPrevBlock;
      block.hashMerkleRoot = hashMerkleRoot;
      block.nTime          = nTime;
      block.nBits          = nBits;
      block.nNonce         = nNonce;
      return block;
    }

    /**
     * Obtain a JSON representation of the block's content.
     */
    Object ToValue();

    /**
     * Obtain a textual JSON representation of the block's content.
     */
    std::string ToString();

    /**
     * Log a textual JSON representation of the block's content.
     */
    void print()
    {
      shcoind_log(ToString().c_str());
#if 0
      fprintf(stderr, "CBlock(iface=%d, hash=%s, PoW=%s, ver=%d, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%lu)\n",
          ifaceIndex, GetHash().GetHex().c_str(),
          GetPoWHash().ToString().substr(0,20).c_str(),
          nVersion,
          hashPrevBlock.GetHex().c_str(),
          hashMerkleRoot.GetHex().c_str(),
          nTime, nBits, nNonce,
          vtx.size());
      for (unsigned int i = 0; i < vtx.size(); i++)
      {
        fprintf(stderr, "\t");
        vtx[i].print();
      }
      fprintf(stderr, "  vMerkleTree:");
      for (unsigned int i = 0; i < vMerkleTree.size(); i++)
        fprintf(stderr, " %s", vMerkleTree[i].GetHex().c_str());
      fprintf(stderr, "\n");
#endif
    }

    virtual bool Truncate() = 0;
    virtual bool ReadBlock(uint64_t nHeight) = 0;
    virtual bool ReadArchBlock(uint256 hash) = 0;
    virtual bool CheckBlock() = 0;
    virtual bool SetBestChain(CTxDB& txdb, CBlockIndex* pindexNew) = 0;
    virtual bool ConnectBlock(CTxDB& txdb, CBlockIndex* pindex) = 0;
    virtual bool IsBestChain() = 0;
    virtual unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast) = 0;
    virtual CScript GetCoinbaseFlags() = 0;
    virtual bool AcceptBlock() = 0;
    virtual bool IsOrphan() = 0;
    virtual bool AddToBlockIndex() = 0;
    virtual void InvalidChainFound(CBlockIndex* pindexNew) = 0;
    virtual bool VerifyCheckpoint(int nHeight) = 0;
    virtual uint64_t GetTotalBlocksEstimate() = 0;
    virtual bool DisconnectBlock(CTxDB& txdb, CBlockIndex* pindex) = 0;

  protected:
    virtual bool SetBestChainInner(CTxDB& txdb, CBlockIndex *pindexNew) = 0;
};

/**
 * Obtain a blank block template for a coin interface.
 */
CBlock *GetBlankBlock(CIface *iface);

/** The block chain is a tree shaped structure starting with the
 * genesis block at the root, with each block potentially having multiple
 * candidates to be the next block.  pprev and pnext link a path through the
 * main/longest chain.  A blockindex may have multiple pprev pointing back
 * to it, but pnext will only point forward to the longest branch, or will
 * be null if the block is not part of the longest chain.
 */
class CBlockIndex
{
  public:
    const uint256* phashBlock;
    CBlockIndex* pprev;
    CBlockIndex* pnext;
    //    unsigned int nFile;
    //    unsigned int nBlockPos;
    int nHeight;
    CBigNum bnChainWork;

    // block header
    int nVersion;
    uint256 hashMerkleRoot;
    unsigned int nTime;
    unsigned int nBits;
    unsigned int nNonce;


    CBlockIndex()
    {
      phashBlock = NULL;
      pprev = NULL;
      pnext = NULL;
      //       nFile = 0;
      //        nBlockPos = 0;
      nHeight = 0;
      bnChainWork = 0;

      nVersion       = 0;
      hashMerkleRoot = 0;
      nTime          = 0;
      nBits          = 0;
      nNonce         = 0;
    }

#if 0
    CBlockIndex(unsigned int nFileIn, unsigned int nBlockPosIn, CBlock& block)
    {
      phashBlock = NULL;
      pprev = NULL;
      pnext = NULL;
      nFile = nFileIn;
      nBlockPos = nBlockPosIn;
      nHeight = 0;
      bnChainWork = 0;

      nVersion       = block.nVersion;
      hashMerkleRoot = block.hashMerkleRoot;
      nTime          = block.nTime;
      nBits          = block.nBits;
      nNonce         = block.nNonce;
    }
#endif

    CBlockIndex(CBlock& block)
    {
      phashBlock = NULL;
      pprev = NULL;
      pnext = NULL;
      //        nFile = nFileIn;
      //       nBlockPos = nBlockPosIn;
      nHeight = 0;
      bnChainWork = 0;

      nVersion       = block.nVersion;
      hashMerkleRoot = block.hashMerkleRoot;
      nTime          = block.nTime;
      nBits          = block.nBits;
      nNonce         = block.nNonce;
    }

    CBlockHeader GetBlockHeader() const
    {
      CBlockHeader block;
      block.nVersion       = nVersion;
      if (pprev)
        block.hashPrevBlock = pprev->GetBlockHash();
      block.hashMerkleRoot = hashMerkleRoot;
      block.nTime          = nTime;
      block.nBits          = nBits;
      block.nNonce         = nNonce;
      return block;
    }

    uint256 GetBlockHash() const
    {
      return *phashBlock;
    }

    int64 GetBlockTime() const
    {
      return (int64)nTime;
    }

    CBigNum GetBlockWork() const
    {
      CBigNum bnTarget;
      bnTarget.SetCompact(nBits);
      if (bnTarget <= 0)
        return 0;
      return (CBigNum(1)<<256) / (bnTarget+1);
    }

    //bool IsInMainChain() const;
    bool IsInMainChain(int ifaceIndex) const;

    bool CheckIndex() const
    {
      return true; // CheckProofOfWork(GetBlockHash(), nBits);
    }

    enum { nMedianTimeSpan=11 };

    int64 GetMedianTimePast() const
    {
      int64 pmedian[nMedianTimeSpan];
      int64* pbegin = &pmedian[nMedianTimeSpan];
      int64* pend = &pmedian[nMedianTimeSpan];

      const CBlockIndex* pindex = this;
      for (int i = 0; i < nMedianTimeSpan && pindex; i++, pindex = pindex->pprev)
        *(--pbegin) = pindex->GetBlockTime();

      std::sort(pbegin, pend);
      return pbegin[(pend - pbegin)/2];
    }

    int64 GetMedianTime() const
    {
      const CBlockIndex* pindex = this;
      for (int i = 0; i < nMedianTimeSpan/2; i++)
      {
        if (!pindex->pnext)
          return GetBlockTime();
        pindex = pindex->pnext;
      }
      return pindex->GetMedianTimePast();
    }



    std::string ToString() const
    {
      return strprintf("CBlockIndex(nprev=%08x, pnext=%08x, nHeight=%d, merkle=%s, hashBlock=%s)",
          pprev, pnext, nHeight,
          hashMerkleRoot.ToString().c_str(),
          GetBlockHash().ToString().c_str());
    }

    void print() const
    {
      printf("%s\n", ToString().c_str());
    }
};

class USDE_CTxMemPool;
class USDEBlock : public CBlock
{
public:
    // header
    static const int CURRENT_VERSION=1;
    static USDE_CTxMemPool mempool; 
    static CBlockIndex *pindexBest;
    static CBlockIndex *pindexGenesisBlock;// = NULL;
    static CBigNum bnBestChainWork;// = 0;
    static CBigNum bnBestInvalidWork;// = 0;
    static int64 nTimeBestReceived ;//= 0;

    static int64 nTargetTimespan;
    static int64 nTargetSpacing;

    USDEBlock()
    {
        ifaceIndex = USDE_COIN_IFACE;
        SetNull();
    }
    USDEBlock(const CBlock &block)
    {
        ifaceIndex = USDE_COIN_IFACE;
        SetNull();
        *((CBlock*)this) = block;
    }

    void SetNull()
    {
      nVersion = USDEBlock::CURRENT_VERSION;
      CBlock::SetNull();

    }

    bool SetBestChain(CTxDB& txdb, CBlockIndex* pindexNew);
    void InvalidChainFound(CBlockIndex* pindexNew);
    unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast);
    bool AcceptBlock();
    bool IsBestChain();
    CScript GetCoinbaseFlags();
    bool AddToBlockIndex();
    bool ConnectBlock(CTxDB& txdb, CBlockIndex* pindex);
    bool CheckBlock();
    bool ReadBlock(uint64_t nHeight);
    bool ReadArchBlock(uint256 hash);
    bool IsOrphan();
    bool Truncate();
    bool VerifyCheckpoint(int nHeight);
    uint64_t GetTotalBlocksEstimate();
    bool DisconnectBlock(CTxDB& txdb, CBlockIndex* pindex);

  protected:
    bool SetBestChainInner(CTxDB& txdb, CBlockIndex *pindexNew);
};

#if 0
class GMCBlock : public CBlock
{
public:
    // header
    static const int CURRENT_VERSION=2;

    GMCBlock()
    {
//        ifaceIndex = GMC_COIN_IFACE;
        SetNull();
    }

    void SetNull()
    {
      nVersion = GMCBlock::CURRENT_VERSION;
      CBlock::SetNull();
    }
};
#endif


class CTxMemPool
{
public:
    mutable CCriticalSection cs;
    std::map<uint256, CTransaction> mapTx;
    std::map<COutPoint, CInPoint> mapNextTx;

    virtual bool accept(CTxDB& txdb, CTransaction &tx, bool fCheckInputs, bool* pfMissingInputs) = 0;

    unsigned long size()
    {
        LOCK(cs);
        return mapTx.size();
    }

    bool exists(uint256 hash)
    {
        return (mapTx.count(hash) != 0);
    }

    CTransaction& lookup(uint256 hash)
    {
        return mapTx[hash];
    }

    virtual void queryHashes(std::vector<uint256>& vtxid) = 0;
};

blkidx_t *GetBlockTable(int ifaceIndex);

/** Describes a place in the block chain to another node such that if the
 * other node doesn't have the same branch, it can find a recent common trunk.
 * The further back it is, the further before the fork it may be.
 */
class CBlockLocator
{
  protected:
    std::vector<uint256> vHave;
    mutable int ifaceIndex;
  public:

    CBlockLocator(int ifaceIndexIn)
    {
      ifaceIndex = ifaceIndexIn;
    }

    explicit CBlockLocator(int ifaceIndexIn, const CBlockIndex* pindex)
    {
      ifaceIndex = ifaceIndexIn;
      Set(pindex);
    }

    explicit CBlockLocator(int ifaceIndexIn, uint256 hashBlock)
    {
      ifaceIndex = ifaceIndexIn;
      blkidx_t *blockIndex = GetBlockTable(ifaceIndex); 
      std::map<uint256, CBlockIndex*>::iterator mi = blockIndex->find(hashBlock);
      if (mi != blockIndex->end())
        Set((*mi).second);
    }

    CBlockLocator(int ifaceIndexIn, const std::vector<uint256>& vHaveIn)
    {
      ifaceIndex = ifaceIndex;
      vHave = vHaveIn;
    }

    IMPLEMENT_SERIALIZE
      (
       if (!(nType & SER_GETHASH))
       READWRITE(nVersion);
       READWRITE(vHave);
      )

      void SetNull()
      {
        vHave.clear();
      }

    bool IsNull()
    {
      return vHave.empty();
    }

    void Set(const CBlockIndex* pindex);

    int GetDistanceBack();

    CBlockIndex* GetBlockIndex();

    uint256 GetBlockHash();

    int GetHeight();
};

/** Used to marshal pointers into hashes for db storage. */
class CDiskBlockIndex : public CBlockIndex
{
public:
    uint256 hashPrev;
    uint256 hashNext;

    CDiskBlockIndex()
    {
        hashPrev = 0;
        hashNext = 0;
    }

    explicit CDiskBlockIndex(CBlockIndex* pindex) : CBlockIndex(*pindex)
    {
        hashPrev = (pprev ? pprev->GetBlockHash() : 0);
        hashNext = (pnext ? pnext->GetBlockHash() : 0);
    }

    IMPLEMENT_SERIALIZE
    (
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);

        READWRITE(hashNext);
/*
        READWRITE(nFile);
        READWRITE(nBlockPos);
*/
        READWRITE(nHeight);

        // block header
        READWRITE(this->nVersion);
        READWRITE(hashPrev);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);
    )

    uint256 GetBlockHash() const
    {
        CBlockHeader block;
        block.nVersion        = nVersion;
        block.hashPrevBlock   = hashPrev;
        block.hashMerkleRoot  = hashMerkleRoot;
        block.nTime           = nTime;
        block.nBits           = nBits;
        block.nNonce          = nNonce;
        return block.GetHash();
    }


    std::string ToString() const
    {
        std::string str = "CDiskBlockIndex(";
        str += CBlockIndex::ToString();
        str += strprintf("\n                hashBlock=%s, hashPrev=%s, hashNext=%s)",
            GetBlockHash().ToString().c_str(),
            hashPrev.ToString().substr(0,20).c_str(),
            hashNext.ToString().substr(0,20).c_str());
        return str;
    }

    void print() const
    {
        printf("%s\n", ToString().c_str());
    }
};


#include "bloom.h"


CBlock *GetBlockByHeight(CIface *iface, int nHeight);

CBlock *GetBlockByHash(CIface *iface, const uint256 hash);

CBlock *GetBlockByTx(CIface *iface, const uint256 hash);

CBlock *CreateBlockTemplate(CIface *iface);

CTxMemPool *GetTxMemPool(CIface *iface);

bool ProcessBlock(CNode* pfrom, CBlock* pblock);

int GetBestHeight(CIface *iface);

int GetBestHeight(int ifaceIndex);

bool IsInitialBlockDownload(int ifaceIndex);

uint256 GetBestBlockChain(CIface *iface);

CBlockIndex *GetGenesisBlockIndex(CIface *iface);


void SetBestBlockIndex(CIface *iface, CBlockIndex *pindex);

void SetBestBlockIndex(int ifaceIndex, CBlockIndex *pindex);

CBlockIndex *GetBestBlockIndex(CIface *iface);

CBlockIndex *GetBestBlockIndex(int ifaceIndex);

void CloseBlockChain(CIface *iface);

bool VerifyTxHash(CIface *iface, uint256 hashTx);

CBlock *GetArchBlockByHash(CIface *iface, const uint256 hash);

uint256 GetGenesisBlockHash(int ifaceIndex);

bool core_AcceptBlock(CBlock *pblock);

CBlockIndex *GetBlockIndexByHeight(int ifaceIndex, unsigned int nHeight);

bool core_DisconnectBlock(CTxDB& txdb, CBlockIndex* pindex, CBlock *pblock);


#endif /* ndef __SERVER_BLOCK_H__ */




