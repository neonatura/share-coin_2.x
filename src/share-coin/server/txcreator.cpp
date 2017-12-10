
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

#include "shcoind.h"
#include "wallet.h"
#include "walletdb.h"
#include "crypter.h"
#include "ui_interface.h"
#include "base58.h"
#include "chain.h"
#include "txsignature.h"
#include "txmempool.h"
#include "txfeerate.h"
#include "txcreator.h"

using namespace std;





void CTxCreator::SetAccount(string strAccountIn)
{
  strFromAccount = strAccountIn;
  fAccount = true;
}


bool CTxCreator::AddInput(CWalletTx *tx, unsigned int n)
{
  int64 nValue;

  if (n < 0 || n >= tx->vout.size()) {
    strError = "An invalid coin input was specified.";
    return (false);
  }

  if (HaveInput(tx, n)) {
    return (false); /* dup */
  }

  nCredit += tx->vout[n].nValue;
  nDepth += tx->GetDepthInMainChain(pwallet->ifaceIndex);
  setInput.insert(setInput.end(), make_pair(tx, n));

  return (true);
}

bool CTxCreator::HaveInput(CWalletTx *tx, unsigned int n)
{
  bool bFound = false;

  BOOST_FOREACH(const PAIRTYPE(CWalletTx *,unsigned int)& coin, setInput) {
    const CWalletTx *wtx = coin.first;
    unsigned int wtx_n = coin.second;

    if (wtx_n == n && wtx->GetHash() == tx->GetHash()) {
      bFound = true;
      break;
    }
  }

  return (bFound);
}


/**
 * @param scriptPubKey The destination script receiving the extended input reference.
 */
bool CTxCreator::AddExtTx(CWalletTx *tx, const CScript& scriptPubKey, int64 nTxFee)
{
  CIface *iface = GetCoinByIndex(pwallet->ifaceIndex);
  int64 nTxValue;
  int64 nValue;
  int nTxOut;

  nTxOut = IndexOfExtOutput(*tx);
  if (nTxOut == -1) {
    strError = "An extended transaction must be specified.";
    return (false);
  }

  /* value left from previous extended transaction. */
  nTxValue = tx->vout[nTxOut].nValue;
  nTxFee = MAX(0, MIN(nTxValue - MIN_TX_FEE(iface), nTxFee));
  nTxFee = MAX(nTxFee, MIN_TX_FEE(iface));
  nValue = nTxValue - nTxFee;

  if (!MoneyRange(iface, nValue)) {
    strError = "Too large of a transaction fee was required.";
    return (false);
  }
  if (!MoneyRange(iface, nTxFee)) {
    strError = "An insufficient transaction fee was allocated.";
    return (false);
  }

  /* ext tx input */
  AddInput(tx, nTxOut);

  /* add tx output */
  AddOutput(scriptPubKey, nValue, true);

  /* ext tx fee */
  SetMinFee(nTxFee);

  return (true);
}

bool CTxCreator::AddOutput(const CPubKey& pubkey, int64 nValue, bool fInsert)
{

  if (!pubkey.IsValid()) {
    strError = "The output destination address is invalid.";
    return (false);
  }

  return (AddOutput(pubkey.GetID(), nValue, fInsert));
}

bool CTxCreator::AddOutput(const CTxDestination& address, int64 nValue, bool fInsert)
{
  CScript script;
  script.SetDestination(address);
  return (AddOutput(script, nValue, fInsert));
}


bool CTxCreator::AddOutput(CScript scriptPubKey, int64 nValue, bool fInsert)
{
  CIface *iface = GetCoinByIndex(pwallet->ifaceIndex);

  if (scriptPubKey.size() == 0) {
    strError = "A valid destination must be specified.";
    return (false);
  }

  if (!MoneyRange(iface, nValue)) {
    strError = "An invalid coin output value was specified.";
    return (false);
  }
  if (nValue < MIN_INPUT_VALUE(iface)) {
    strError = "Output value is less than minimum allowed.";
    return (false);
  }

  if (fInsert) {
    vout.insert(vout.begin(), CTxOut(nValue, scriptPubKey));
  } else if (vout.size() == 0 || (-1 != IndexOfExtOutput(*this))) {
    vout.push_back(CTxOut(nValue, scriptPubKey));
  } else {
    vector<CTxOut>::iterator position = vout.begin()+GetRandInt(vout.size());
    vout.insert(position, CTxOut(nValue, scriptPubKey));
  }

  nDebit += nValue;
 
  return (true);
}

bool CTxCreator::SetChange(const CPubKey& scriptPubKey)
{
  changePubKey = scriptPubKey;
}

void CTxCreator::SetMinFee(int64 nMinFeeIn)
{
  if (nMinFeeIn < 0)
    return;
  nMinFee = nMinFeeIn;
}


int64 CTxCreator::CalculateFee()
{
  CIface *iface = GetCoinByIndex(pwallet->ifaceIndex);
  int64 nFee;

#if 0
  int64 nFee = nMinFee;
  if (nMinFee == 0) {
    int64 nBytes = pwallet->GetVirtualTransactionSize(*this);
    if (nBytes < 10000) {
      double dPriority  = GetPriority(nBytes);
      if (pwallet->AllowFree(dPriority))
        return (0);
    }
  }
#endif

  /* core */
  nFee = pwallet->CalculateFee(*this, nMinFee);

  /* rolling */
  CBlockPolicyEstimator *fee = GetFeeEstimator(iface);
  if (fee) {
    static const int confTarget = 2;
    int64 nSoftFee;
    int64 nBytes;

    nBytes = pwallet->GetVirtualTransactionSize(*this);
    nSoftFee = fee->estimateSmartFee(confTarget, NULL).GetFee(nBytes);
    if (nSoftFee > MIN_TX_FEE(iface) &&
        nSoftFee < MAX_TX_FEE(iface))
      nFee = MAX(nFee, nSoftFee);
  }

  return (nFee);
}

void CTxCreator::CreateChangeAddr()
{
  changePubKey = pwallet->GenerateNewKey();
}

bool CTxCreator::Generate()
{
  CIface *iface = GetCoinByIndex(pwallet->ifaceIndex);
  CWallet *wallet = GetWallet(iface);
  int64 nFee;
  int nBestHeight;
  bool ok;
    cbuff sigDummy(72, '\000');
    cbuff sigPub(64, '\000');

  if (vout.size() == 0) {
    strError = "No outputs have been specified.";
    return (false);
  }

  /* mitigate sniping */
  nBestHeight = GetBestHeight(iface);
  nLockTime = nBestHeight - 60;

  /* establish "coin change" destination address */
  int ext_idx = IndexOfExtOutput(*this);
  if (fAccount && (ext_idx == -1) && !changePubKey.IsValid()) {
    CPubKey changePubKey;

    ok = pwallet->GetMergedPubKey(strFromAccount, "change", changePubKey);
    if (ok) {
      SetChange(changePubKey);
    }
  }
  if (!changePubKey.IsValid()) {
    /* pull a reserved key from the pool. */
    CreateChangeAddr();
#if 0
    /* done in Send() instead */
    if (fAccount)
      pwallet->SetAddressBookName(changePubKey.GetID(), strFromAccount);
#endif
  }

  vector<COutput> vCoins;
  if (fAccount) {
    wallet->AvailableAccountCoins(strFromAccount, vCoins);
  } else {
    wallet->AvailableCoins(vCoins);
  }

  nFee = MIN_RELAY_TX_FEE(iface);
  set<pair<const CWalletTx*,unsigned int> > setCoinsCopy;
  int64 nTotCredit = nCredit;
  while (nDebit + nFee > nTotCredit) {
    setCoinsCopy.clear();

    set<pair<const CWalletTx*,unsigned int> > setCoins;
    int64 nTotalValue = (nDebit + nFee - nTotCredit);
    int64 nValueIn = 0;

    /* add inputs to use */
/* todo: SetSelectMode(SELECT_AVG, SELECT_MINCONF).. */
    ok = SelectCoins_Avg((nDebit + nFee), vCoins, setCoins, nValueIn);
    if (!ok) {
      strError = "Insufficient input coins to fund transaction.";
      return (false);
    }

    nValueIn = 0;
    BOOST_FOREACH(const PAIRTYPE(const CWalletTx*,unsigned int)& coin, setCoins) {
      CWalletTx *wtx = (CWalletTx *)coin.first;
      unsigned int n = coin.second;

      if (n < 0 || n >= wtx->vout.size())
        continue; /* inval */

      if (HaveInput(wtx, n))
        continue; /* already used */

      setCoinsCopy.insert(setCoinsCopy.end(), make_pair(wtx, n));
      nValueIn += wtx->vout[n].nValue;
    }
    if (nValueIn < nTotalValue) {
      strError = "Insufficient input coins to fund transaction.";
      return (false);
    } 

    CWalletTx t_wtx(wallet);

    /* add inputs */
    int nCount = 0;
    nTotCredit = nCredit;
    BOOST_FOREACH(const PAIRTYPE(const CWalletTx*,unsigned int)& coin, setCoinsCopy) {
      CWalletTx *wtx = (CWalletTx *)coin.first;
      unsigned int n = coin.second;

      CTxIn in = CTxIn(coin.first->GetHash(), coin.second, 
          CScript(), std::numeric_limits<unsigned int>::max()-1);
      in.scriptSig << sigDummy << sigPub;
      t_wtx.vin.push_back(in);

      nCount++;
      nTotCredit += wtx->vout[n].nValue;
      if (nDebit + nFee <= nTotCredit)
        break;
    }
    if (!nCount) {
      /* prevent an endless loop */
      strError = "Insufficient input coins to fund transaction.";
      return (false);
    }

    /* add outputs */
    BOOST_FOREACH(const CTxOut& out, vout) {
      t_wtx.vout.insert(t_wtx.vout.end(), out);
    }

    int64 nChange = (nTotCredit - nDebit - nFee);
    if (nChange >= CENT) {
      CScript script;

      script.SetDestination(changePubKey.GetID());
      t_wtx.vout.insert(t_wtx.vout.end(), CTxOut(nChange, script));
    }

    nFee = wallet->CalculateFee(t_wtx, nMinFee);
  }
  if (!MoneyRange(iface, nFee)) {
    strError = "The calculated fee is out-of-range.";
    return (false);
  }


  /* parse inputs */
  BOOST_FOREACH(const PAIRTYPE(const CWalletTx*,unsigned int)& coin, setCoinsCopy) {
    CWalletTx *wtx = (CWalletTx *)coin.first;
    unsigned int n = coin.second;

    if (!AddInput(wtx, n)) {
      strError = "A wallet transaction is invalid.";
      return (false);
    }
  }

  /* handle coin change */
  int64 nChange = (nCredit - nDebit - nFee);
  if (nChange >= MIN_INPUT_VALUE(iface) &&
      nChange >= MIN_TX_FEE(iface) &&
      nChange >= CENT) {
    AddOutput(changePubKey, nChange); 
  }

  /* add inputs to transaction */
  vin.clear();
  BOOST_FOREACH(const PAIRTYPE(CWalletTx *,unsigned int)& coin, setInput) {
    vin.push_back(CTxIn(coin.first->GetHash(), coin.second, 
          CScript(), std::numeric_limits<unsigned int>::max()-1));
  }

  /* redundantly check before signing as signing takes the longest time */
  unsigned int nWeight = pwallet->GetTransactionWeight(*this);
  if (nWeight >= MAX_TRANSACTION_WEIGHT(iface)) {
    strError = "The transaction size exceeds the maximum complexity allowed.";
    return (false);
  }

  /* sign inputs */
  unsigned int nIn = 0;
  BOOST_FOREACH(const PAIRTYPE(CWalletTx *,unsigned int)& coin, setInput) {
    CSignature sig(pwallet->ifaceIndex, this, nIn);
    if (!sig.SignSignature(*coin.first)) {
      return error(SHERR_INVAL, "Generate: error signing a transaction input.");
    }

    nIn++;
  }

  /* ensure transaction does not breach a defined size limitation. */
  nWeight = pwallet->GetTransactionWeight(*this);
  if (nWeight >= MAX_TRANSACTION_WEIGHT(iface)) {
    strError = "The transaction size exceeds the maximum complexity allowed.";
    return (false);
  }

/* DEBUG: GetSigOpCost() */
  if (GetLegacySigOpCount() > MAX_BLOCK_SIGOPS(iface)/5) {
    strError = "The number of transaction signature operations exceed the maximum complexity allowed.";
    return (false);
  }

  if (!CheckTransaction(pwallet->ifaceIndex)) {
    strError = "The transaction integrity is invalid.";
    return (false);
  }

#if 0
  /* redundant */
  int64 nInputValue = GetValueIn(mapInputs);
  int64 nOutputValue = GetValueOut();
  if (nInputValue < nOutputValue) {
    error(SHERR_INVAL, "CTxCreator: tx \"%s\" has input value (%f) lower than output value (%f).", GetHash().GetHex().c_str(), (double)nInputValue/COIN, (double)nOutputValue/COIN);
    continue; 
  }
#endif

  fGenerate = true;
  return (true);
}

bool CTxCreator::Send()
{

  if (!fGenerate) {
    /* ensure transaction has been finalized. */
    bool ok = Generate();
    if (!ok)
      return (false);
  }

#if 0
  if (nReserveIndex != -1) {
    /* a reserved key was created in the making of this transaction. */
    pwallet->KeepKey(nReserveIndex);
    if (fAccount) {
      pwallet->SetAddressBookName(changePubKey.GetID(), strFromAccount);
    }
  }
#endif
  if (fAccount) {
    pwallet->SetAddressBookName(changePubKey.GetID(), strFromAccount);
  }

  /* fill vtxPrev by copying from previous transactions vtxPrev */
  pwallet->AddSupportingTransactions(*this);
  fTimeReceivedIsTxTime = true;

  if (!pwallet->CommitTransaction(*this)) {
    strError = "An error occurred while commiting the transaction.";
    return (false);
  }

  return (true);
}

bool CTxCreator::Verify()
{

  if (!fGenerate) {
    bool ok = Generate();
    if (!ok)
      return (false);
  }

  return (CheckTransaction(pwallet->ifaceIndex));
}

size_t CTxCreator::GetSerializedSize()
{
  const CTransaction& tx = *((CTransaction *)this);
  return (pwallet->GetVirtualTransactionSize(tx));
}

bool CTxCreator::AddInput(uint256 hashTx, unsigned int n)
{
  

  if (pwallet->mapWallet.count(hashTx) == 0)
    return (false); /* is not in wallet. */

  CWalletTx& wtx = pwallet->mapWallet[hashTx];
  return (AddInput(&wtx, n));
}

double CTxCreator::GetPriority(int64 nBytes)
{
  CIface *iface = GetCoinByIndex(pwallet->ifaceIndex);
  double dPriority;

  if (nBytes == 0)
    nBytes = ::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION(iface));

  dPriority = 0;
  BOOST_FOREACH(const PAIRTYPE(CWalletTx *,unsigned int)& coin, setInput) {
    CWalletTx *txPrev = coin.first;
    unsigned int n = coin.second;

    if (n >= txPrev->vout.size())
      continue;

    const CTxOut& out = txPrev->vout[n];
    dPriority += (double)out.nValue * 
      (double)txPrev->GetDepthInMainChain(pwallet->ifaceIndex);
  }
  dPriority /= nBytes;

  return (dPriority);
}

bool CTxBatchCreator::CreateBatchTx()
{
#if 0
  static const cbuff sigDummy(72, '\000');
  static const cbuff sigPub(64, '\000');
#endif
  CIface *iface = GetCoinByIndex(pwallet->ifaceIndex);
  vector<CTxIn> vIn; /* out */
  vector<CTxOut> vOut; /* out */
  bool ok;

  /* init tx */
  CTxCreator ret_tx(pwallet, strFromAccount);  

  /* fake change output */
  int64 nFee = nMaxFee;
  int64 nTotCredit = 0;
  int64 nTotDebit = (nOutValue - nBatchValue);

  int64 nTotSelect = 0;
  BOOST_FOREACH(const PAIRTYPE(const CWalletTx*,unsigned int)& coin, setCoins) {
    CWalletTx *wtx = (CWalletTx *)coin.first;
    unsigned int n = coin.second;

#if 0
    CTxIn in = CTxIn(coin.first->GetHash(), coin.second);
    if (find(vBatchIn.begin(), vBatchIn.end(), in) != vBatchIn.end())
      continue; /* already processed */
#endif

    nTotSelect += wtx->vout[n].nValue; 
  }

  if (nTotSelect < (nTotDebit + MIN_TX_FEE(iface))) {
    /* insufficient funds to proceed. */
    strError = "insufficient funds in account";
    return (error(SHERR_INVAL, "CreateBatchTx: insufficient funds (%f < %f)", (double)nTotSelect/COIN, (double)nTotDebit/COIN));
  }

  bool fUpdate = false;
  BOOST_FOREACH(const PAIRTYPE(const CWalletTx*,unsigned int)& coin, setCoins) {
    CWalletTx *wtx = (CWalletTx *)coin.first;
    unsigned int n = coin.second;

    CTxIn in = CTxIn(coin.first->GetHash(), coin.second);
#if 0
    if (find(vBatchIn.begin(), vBatchIn.end(), in) != vBatchIn.end())
      continue; /* already processed */
    in.scriptSig << sigDummy << sigPub;
#endif
    ret_tx.AddInput(in.prevout.hash, in.prevout.n);
    vIn.push_back(in);
    nTotCredit += wtx->vout[n].nValue;

    fUpdate = true;

    if (vIn.size() > 10) {
      int64 nSigOp = ret_tx.GetLegacySigOpCount();
      if (nSigOp > nMaxSigOp) {
        break;
      }
    }

    int64 nBytes = ::GetSerializeSize(ret_tx, SER_NETWORK, PROTOCOL_VERSION(iface) | SERIALIZE_TRANSACTION_NO_WITNESS);
    nBytes += (146 * ret_tx.vin.size()) + 600;
    if (nBytes > nMaxTxSize) {
      break;
}

    int64 nCurFee = ((nBytes / 1024) + 2) * MIN_TX_FEE(iface);
    nFee = (nFee + nCurFee) / 2;
    if (nFee > nMaxFee) {
      break;
}

    /* check whether we have aquired enough coins. */
    if ((nTotCredit - nFee) > nTotDebit) {
      break;
    }

  }
  if (!fUpdate)
    return (false);
 
  nTotDebit = MIN(nTotDebit, (nTotCredit - nFee)); /* sanity */
  nTotDebit = MIN(nTotDebit, (nOutValue - nBatchValue)); /* batch left */
  if (nTotDebit <= CENT + MIN_TX_FEE(iface)) {
    return (false);
  }

  /* output */
  ret_tx.AddOutput(scriptPub, nTotDebit);

  if (!ret_tx.Generate()) {
    return (error(SHERR_INVAL, "CTxBatchCreator.Generate: error generating tx."));
  }

  /* mark inputs as processed */
  BOOST_FOREACH(const CTxIn& in, vIn) {
    set<pair<const CWalletTx *, unsigned int> >::const_iterator it;
    for (it = setCoins.begin(); it != setCoins.end(); ++it) {
      CWalletTx *wtx = (CWalletTx *)it->first;
      const  unsigned int n = it->second;
      if (in.prevout.hash == wtx->GetHash() && (n == in.prevout.n)) {
        break;
      }
    }
    if (it != setCoins.end()) {
      setCoins.erase(it);
    }
  }

  nBatchValue += nTotDebit;

  /* add to tx commit list */ 
  vTxList.push_back(ret_tx);
  return (true);
}

bool CTxBatchCreator::Generate()
{
  CIface *iface = GetCoinByIndex(pwallet->ifaceIndex);

  setCoins.clear();

  /* select inputs to use */
  int64 nSelect = nOutValue + nMaxFee;
  int64 nValueIn = 0;
  vector<COutput> vCoins;
  pwallet->AvailableAccountCoins(strFromAccount, vCoins);
  SelectCoins_Avg(nSelect, vCoins, setCoins, nValueIn);

  int64 nMaxValue = 
    MAX(MIN_TX_FEE(iface) * 2, nOutValue - CENT - MIN_TX_FEE(iface));
  while (nBatchValue < nMaxValue) {
    bool ok = CreateBatchTx(); 
    if (!ok)
      break;
  }

  if (vTxList.size() == 0) {
    return (false);
  }

  fGenerate = true;
  return (true);
}

bool CTxBatchCreator::Send()
{

  if (!fGenerate) {
    /* ensure transaction has been finalized. */
    bool ok = Generate();
    if (!ok)
      return (false);
  }

  if (fAccount) {
    pwallet->SetAddressBookName(changePubKey.GetID(), strFromAccount);
/* DEBUG: TODO: save wallet to disk if updated */
  }

  vector<CWalletTx> vCommitList;
  vector<CWalletTx>& tx_list = vTxList;
  BOOST_FOREACH(CWalletTx& wtx, vTxList) {
    /* fill vtxPrev by copying from previous transactions vtxPrev */
    pwallet->AddSupportingTransactions(wtx);
    wtx.fTimeReceivedIsTxTime = true;
    if (pwallet->CommitTransaction(wtx)) {
      vCommitList.push_back(wtx);
    }
  }
  if (vCommitList.size() == 0)
    return (false); /* all transactions failed to commit */
  vTxList = vCommitList;

  return (true);
}


