
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
#include "block.h"
#include "db.h"
#include <vector>

using namespace std;

//map<uint256, CBlockIndex*> tableBlockIndex[MAX_COIN_IFACE];
blkidx_t tableBlockIndex[MAX_COIN_IFACE];
//vector <bc_t *> vBlockChain;

blkidx_t *GetBlockTable(int ifaceIndex)
{
  if (ifaceIndex < 1 || ifaceIndex >= MAX_COIN_IFACE)
    return (NULL);
  return (&tableBlockIndex[ifaceIndex]);
}


/**
 * Opens a specific database of block records.
 */
bc_t *GetBlockChain(CIface *iface)
{

  if (!iface->bc_block) {
    char name[4096];

    sprintf(name, "%s_block", iface->name);
    bc_open(name, &iface->bc_block);
  }

  return (iface->bc_block);
}

/**
 * Opens a specific database of block references. 
 */
bc_t *GetBlockTxChain(CIface *iface)
{

  if (!iface->bc_tx) {
    char name[4096];

    sprintf(name, "%s_tx", iface->name);
    bc_open(name, &iface->bc_tx);
  }

  return (iface->bc_tx);
}

/**
 * Closes all open block record databases.
 */
void CloseBlockChains(void)
{
  CIface *iface;
  int idx;

  for (idx = 1; idx < MAX_COIN_IFACE; idx++) {
    iface = GetCoinByIndex(idx);
    if (!iface)
      continue;

    if (iface->bc_block) {
      bc_close(iface->bc_block);
      iface->bc_block = NULL;
    }
    if (iface->bc_tx) {
      bc_close(iface->bc_tx);
      iface->bc_tx = NULL;
    }
  }

}

#if 0
bc_t *GetBlockChain(char *name)
{
  bc_t *bc;

  for(vector<bc_t *>::iterator it = vBlockChain.begin(); it != vBlockChain.end(); ++it) {
    bc = *it;
    if (0 == strcmp(bc_name(bc), name))
      return (bc);
  }

  bc_open(name, &bc);
  vBlockChain.push_back(bc);

  return (bc);
}

/**
 * Closes all open block record databases.
 */
void CloseBlockChains(void)
{
  bc_t *bc;

  for(vector<bc_t *>::iterator it = vBlockChain.begin(); it != vBlockChain.end(); ++it) {
    bc_t *bc = *it;
    bc_close(bc);
  }
  vBlockChain.clear();

}
#endif


int64 GetInitialBlockValue(int nHeight, int64 nFees)
{
  int64 nSubsidy = 4000 * COIN;

  if ((nHeight % 100) == 1)
  {
    nSubsidy = 100000 * COIN; //100k
  }else if ((nHeight % 50) == 1)
  {
    nSubsidy = 50000 * COIN; //50k
  }else if ((nHeight % 20) == 1)
  {
    nSubsidy = 20000 * COIN; //20k
  }else if ((nHeight % 10) == 1)
  {
    nSubsidy = 10000 * COIN; //10k
  }else if ((nHeight % 5) == 1)
  {
    nSubsidy = 5000 * COIN; //5k
  }

  //limit first blocks to protect against instamine.
  if (nHeight < 2){
    nSubsidy = 24000000 * COIN; // 1.5%
  }else if(nHeight < 500)
  {
    nSubsidy = 100 * COIN;
  }
  else if(nHeight < 1000)
  {
    nSubsidy = 500 * COIN;
  }

  nSubsidy >>= (nHeight / 139604);

  return (nSubsidy + nFees);
}

#if 0
int64 GetBlockValue(int nHeight, int64 nFees)
{
  int64 nSubsidy = 4000 * COIN;
  int base = nHeight;

  if (nHeight < 107500) {
    return (GetInitialBlockValue(nHeight, nFees));
  }

#if CLIENT_VERSION_REVISION > 4
  if (nHeight >= 1675248) {
    /* transition from 1.6bil cap to 1.6tril cap. */
    base /= 9;
  }
#endif

  nSubsidy >>= (base / 139604);

#if CLIENT_VERSION_REVISION > 4
  if (nHeight >= 1675248) {
    /* balance flux of reward. reduces max coin cap to 320bil */
    nSubsidy /= 5;
  }
#endif

  return nSubsidy + nFees;
}
#endif

const CTransaction *CBlock::GetTx(uint256 hash)
{
  BOOST_FOREACH(const CTransaction& tx, vtx)
    if (tx.GetHash() == hash)
      return (&tx);
  return (NULL);
}


bool CTransaction::WriteTx(int ifaceIndex, uint64_t blockHeight)
{
  bc_t *bc = GetBlockTxChain(GetCoinByIndex(ifaceIndex));
  uint256 hash = GetHash();
  char errbuf[1024];
  int txPos;
  int nHeight;
  int err;

  if (!bc) {
    unet_log(ifaceIndex, "CTransaction::WriteTx: error opening tx chain.");
    return (false);
  }

  if (0 == bc_idx_find(bc, hash.GetRaw(), NULL, NULL)) {
    /* transaction reference exists */
    return (true);
  }

  /* reference block height */
  err = bc_append(bc, hash.GetRaw(), &blockHeight, sizeof(blockHeight));
  if (err < 0) {
    sprintf(errbuf, "CTransaction::WriteTx: error writing block reference: %s.", sherrstr(err));
    unet_log(ifaceIndex, errbuf);
    return (false);
  }

fprintf(stderr, "DEBUG: CTransaction::WriteTx[iface #%d]: wrote tx '%s' for block #%d\n", ifaceIndex, hash.GetHex().c_str(), (int)blockHeight);
  return (true);
}

bool CTransaction::ReadTx(int ifaceIndex, uint256 txHash)
{
  uint256 hashBlock; /* dummy var */
  return (ReadTx(ifaceIndex, txHash, hashBlock));
}

bool CTransaction::ReadTx(int ifaceIndex, uint256 txHash, uint256 &hashBlock)
{
  CIface *iface;
  bc_t *bc;
  char errbuf[1024];
  unsigned char *data;
  uint64_t blockHeight;
  size_t data_len;
  int txPos;
  int err;

  SetNull();

  iface = GetCoinByIndex(ifaceIndex);
  if (!iface) {
    sprintf(errbuf, "CTransaction::ReadTx: unable to obtain iface #%d.", ifaceIndex); 
    return error(SHERR_INVAL, errbuf);
  }

  bc = GetBlockTxChain(iface);
  if (!bc) { 
    return error(SHERR_INVAL, "CTransaction::ReadTx: unable to open block tx database."); 
  }

  err = bc_idx_find(bc, txHash.GetRaw(), NULL, &txPos); 
  if (err) {
fprintf(stderr, "DEBUG: CTransaction::ReadTx[iface #%d]: tx hash '%s' not found. [tot-tx:%d] [err:%d]\n", ifaceIndex, txHash.GetHex().c_str(), bc_idx_next(bc), err);
    return (false); /* not an error condition */
}

  err = bc_get(bc, txPos, &data, &data_len);
  if (data_len != sizeof(uint64_t)) {
    sprintf(errbuf, "CTransaction::ReadTx: tx position %d not found.", txPos);
    return error(SHERR_INVAL, errbuf);
  }
  if (data_len != sizeof(uint64_t)) {
    sprintf(errbuf, "CTransaction::ReadTx: block reference has invalid size (%d).", data_len);
    return error(SHERR_INVAL, errbuf);
  }
  memcpy(&blockHeight, data, sizeof(blockHeight));
  free(data);

  CBlock *block = GetBlankBlock(iface);
  if (!block) { 
fprintf(stderr, "DEBUG: CTransaction::ReadTx: error allocating new block\n");
return (false);
}
  if (!block->ReadBlock(blockHeight)) {
fprintf(stderr, "DEBUG: CTransaction::ReadTx: block height %d not valid.\n", blockHeight);
  delete block;
return (false);
  }

  const CTransaction *tx = block->GetTx(txHash);
  if (!tx) {
    sprintf(errbuf, "CTransaction::ReadTx: block height %d does not contain tx.", blockHeight);
    delete block;
    return error(SHERR_INVAL, errbuf);
  }

  Init(*tx);
  hashBlock = block->GetHash();
  delete block;

  return (true);
}

bool CTransaction::ReadFromDisk(CDiskTxPos pos)
{
  int ifaceIndex = pos.nFile;
  CIface *iface = GetCoinByIndex(ifaceIndex);
  bc_t *bc = GetBlockTxChain(GetCoinByIndex(ifaceIndex));
  bc_hash_t b_hash;
  char errbuf[1024];
  uint256 hash;
  int err;

  if (!iface) {
    sprintf(errbuf, "CTransaction::ReadTx: error obtaining coin iface #%d\n", (int)pos.nFile);
    return error(SHERR_INVAL, errbuf);
  }

  err = bc_get_hash(bc, pos.nTxPos, b_hash);
  if (err) {
    sprintf(errbuf, "CTransaction::ReadTx: error obtaining tx index #%u\n", (unsigned int)pos.nTxPos);
    return error(err, errbuf);
  }

  hash.SetRaw(b_hash);
  CBlock *block = GetBlockByTx(iface, hash);
  if (!block) {
    sprintf(errbuf, "CTransaction::ReadTx: error obtaining block by tx\n");
    return error(SHERR_INVAL, errbuf);
}

  const CTransaction *tx = block->GetTx(hash);
  if (!tx) {
    sprintf(errbuf, "CTransaction::ReadTx: block '%s' does not contain tx '%s'.", block->GetHash().GetHex().c_str(), hash.GetHex().c_str());
  delete block;
    return error(SHERR_INVAL, errbuf);
  }

  Init(*tx);
  delete block;

  return (true);
}

#if 0 
bool CTransaction::FillTx(int ifaceIndex, CDiskTxPos &pos)
{
  CIface *iface = GetCoinByIndex(ifaceIndex);
  bc_t *bc = GetBlockTxChain(iface);
  bc_hash_t b_hash;
  char errbuf[1024];
  unsigned char *data;
  uint64_t blockHeight;
  size_t data_len;
  int t_pos;
  int err;

  pos.nFile = ifaceIndex;

  memcpy(b_hash, GetHash().GetRaw(), sizeof(bc_hash_t));
  err = bc_find(bc, b_hash, &t_pos);
  if (err)
    return (false);
  pos.nTxPos = t_pos; 

  err = bc_get(bc, pos.nTxPos, &data, &data_len);
  if (data_len != sizeof(uint64_t)) {
    sprintf(errbuf, "CTransaction::ReadTx: tx position %d not found.", pos.nTxPos);
    unet_log(ifaceIndex, errbuf);
    return (false);
  }
  memcpy(&blockHeight, data, sizeof(blockHeight));
  free(data);
  pos.nBlockPos = blockHeight;

  return (true);
}
#endif


#if 0
CBlock *GetBlockTemplate(int ifaceIndex)
{
  static CBlockIndex* pindexPrev;
  static unsigned int work_id;
  static time_t last_reset_t;
  CWallet *wallet = GetWallet(ifaceIndex);
  CBlock* pblock;
  int reset;

  if (!wallet) {
    unet_log(ifaceIndex, "GetBlocKTemplate: Wallet not initialized.");
    return (NULL);
  }

  CReserveKey reservekey(wallet);

  // Store the pindexBest used before CreateNewBlock, to avoid races
  CBlockIndex* pindexPrevNew = pindexBest;

  pblock = CreateNewBlock(reservekey);
 
  // Need to update only after we know CreateNewBlock succeeded
  pindexPrev = pindexPrevNew;

  pblock->UpdateTime(pindexPrev);
  pblock->nNonce = 0;

  return (pblock);
}
#endif



extern CCriticalSection cs_main;

#if 0
/** Return transaction in tx, and if it was found inside a block, its hash is placed in hashBlock */
bool GetTransaction(CIface *iface, const uint256 &hash, CTransaction &tx, uint256 &hashBlock)
{
  int ifaceIndex = GetCoinIndex(iface);
  {
    LOCK(cs_main);
    {
      LOCK(mempool.cs);
      if (mempool.exists(hash))
      {
        tx = mempool.lookup(hash);
        return true;
      }
    }

    if (tx.ReadTx(GetCoinIndex(iface), hash, hashBlock)) {
      fprintf(stderr, "DEBUG: GetTransaction: OK: read tx chain '%s'\n", tx.GetHash().GetHex().c_str());
      return (true);
    }
    fprintf(stderr, "DEBUG: GetTransaction: WARNING: using C++ CTxIndex\n");

    CTxDB txdb(ifaceIndex, "r");
    CTxIndex txindex;
    if (tx.ReadFromDisk(txdb, COutPoint(hash, 0), txindex))
    {
      CBlock *block;

      iface->block_new(iface, &block); //      USDEBlock block;
      if (block->ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos, false))
        hashBlock = block->GetHash();
      txdb.Close();
      return true;
    }
    txdb.Close();
  }
  return false;
}
#endif

bool GetTransaction(CIface *iface, const uint256 &hash, CTransaction &tx, uint256 &hashBlock)
{
  return (tx.ReadTx(GetCoinIndex(iface), hash, hashBlock));
}

CBlock *GetBlockByHeight(CIface *iface, int nHeight)
{
  int ifaceIndex = GetCoinIndex(iface);
  blkidx_t *blockIndex;
  CTransaction tx;
  CBlock *block;
  int err;
  
  /* sanity */
  if (!iface)
    return (NULL);

  block = GetBlankBlock(iface);
  if (!block)
    return (NULL);

  if (!block->ReadBlock(nHeight))
    return (NULL);

  return (block);
}

CBlock *GetBlockByHash(CIface *iface, const uint256 hash)
{
  int ifaceIndex = GetCoinIndex(iface);
  blkidx_t *blockIndex;
  CBlockIndex *pindex;
  CTransaction tx;
  CBlock *block;
  int err;
  
  /* sanity */
  if (!iface)
    return (NULL);

  blockIndex = GetBlockTable(ifaceIndex);
  if (!blockIndex)
    return (NULL);

  pindex = (*blockIndex)[hash];
  if (!pindex)
    return (NULL);

  /* generate block */
  block = GetBlankBlock(iface);
  if (!block)
    return (NULL);

  if (!block->ReadFromDisk(pindex))
    return (NULL);

  /* verify integrity */
  if (block->GetHash() != hash)
    return (NULL);

  return (block);
}

CBlock *GetBlockByTx(CIface *iface, const uint256 hash)
{
  int ifaceIndex = GetCoinIndex(iface);
  CTransaction tx;
  CBlock *block;
  uint256 hashBlock;
  
  /* sanity */
  if (!iface)
    return (NULL);

  /* figure out block hash */
  if (!tx.ReadTx(GetCoinIndex(iface), hash, hashBlock))
    return (NULL);

  return (GetBlockByHash(iface, hashBlock));
}

CBlock *CreateBlockTemplate(CIface *iface)
{
  int ifaceIndex = GetCoinIndex(iface);
  CBlock *block;
  char errbuf[256];
  int err;

  if (!iface->op_block_templ)
    return (NULL);

  block = NULL;
  err = iface->op_block_templ(iface, &block); 
  if (err) {
    sprintf(errbuf, "CreateBlockTemplate: error creating block template: %s.", sherrstr(err));
    unet_log(ifaceIndex, errbuf);
  }

  return (block);
}

CTxMemPool *GetTxMemPool(CIface *iface)
{
  CTxMemPool *pool;
  int err;

  if (!iface->op_tx_pool) {
    int ifaceIndex = GetCoinIndex(iface);
    unet_log(ifaceIndex, "GetTxMemPool: error obtaining tx memory pool: Operation not supported.");
    return (NULL);
  }

  err = iface->op_tx_pool(iface, &pool);
  if (err) {
    int ifaceIndex = GetCoinIndex(iface);
    char errbuf[256];
    sprintf(errbuf, "GetTxMemPool: error obtaining tx memory pool: %s [sherr %d].", sherrstr(err), err);
    unet_log(ifaceIndex, errbuf);
    return (NULL);
  }

  return (pool);
}






bool ProcessBlock(CNode* pfrom, CBlock* pblock)
{
  CIface *iface = GetCoinByIndex(pblock->ifaceIndex);
  int err;

  if (!iface)
    return (false);

  pblock->originPeer = pfrom;
  err = iface->op_block_process(iface, pblock);
  if (err) {
    char errbuf[1024];

    sprintf(errbuf, "error processing incoming block: %s [sherr %d].", sherrstr(err), err); 
    unet_log(pblock->ifaceIndex, errbuf);
    return (false);
  }

  return (true);
}





bool CTransaction::ClientConnectInputs(int ifaceIndex)
{
  CIface *iface = GetCoinByIndex(ifaceIndex);
  CTxMemPool *pool;

  if (!iface) {
    unet_log(ifaceIndex, "error obtaining coin interface.");
    return (false);
  }

  pool = GetTxMemPool(iface);
  if (!pool) {
    unet_log(ifaceIndex, "error obtaining tx memory pool.");
    return (false);
  }

  if (IsCoinBase())
    return false;

  // Take over previous transactions' spent pointers
  {
    LOCK(pool->cs);
    int64 nValueIn = 0;
    for (unsigned int i = 0; i < vin.size(); i++)
    {
      // Get prev tx from single transactions in memory
      COutPoint prevout = vin[i].prevout;
      if (!pool->exists(prevout.hash))
        return false;
      CTransaction& txPrev = pool->lookup(prevout.hash);

      if (prevout.n >= txPrev.vout.size())
        return false;

      // Verify signature
      if (!VerifySignature(txPrev, *this, i, true, 0))
        return error(SHERR_INVAL, "ConnectInputs() : VerifySignature failed");

      ///// this is redundant with the mempool.mapNextTx stuff,
      ///// not sure which I want to get rid of
      ///// this has to go away now that posNext is gone
      // // Check for conflicts
      // if (!txPrev.vout[prevout.n].posNext.IsNull())
      //     return error("ConnectInputs() : prev tx already used");
      //
      // // Flag outpoints as used
      // txPrev.vout[prevout.n].posNext = posThisTx;

      nValueIn += txPrev.vout[prevout.n].nValue;

      if (!MoneyRange(ifaceIndex, txPrev.vout[prevout.n].nValue) || 
          !MoneyRange(ifaceIndex, nValueIn))
        return error(SHERR_INVAL, "ClientConnectInputs() : txin values out of range");
    }
    if (GetValueOut() > nValueIn)
      return false;
  }

  return true;
}



bool CBlockIndex::IsInMainChain(int ifaceIndex) const
{
  if (pnext)
    return (true);
  CIface *iface = GetCoinByIndex(ifaceIndex);
  if (!iface) return (false);
  CBlock *block = GetBlockByHash(iface, GetBlockHash()); 
  if (!block) return (false);
  bool ret = block->IsBestChain();
  delete block;
  return (ret);
} 


uint256 CBlockLocator::GetBlockHash()
{
  CIface *iface = GetCoinByIndex(ifaceIndex);
  blkidx_t *blockIndex = GetBlockTable(ifaceIndex); 

  // Find the first block the caller has in the main chain
  BOOST_FOREACH(const uint256& hash, vHave)
  {
    std::map<uint256, CBlockIndex*>::iterator mi = blockIndex->find(hash);
    if (mi != blockIndex->end())
    {
      CBlockIndex* pindex = (*mi).second;
      if (pindex->IsInMainChain(ifaceIndex))
        return hash;
    }
  }

  CBlock *block = GetBlockByHeight(iface, 0);
  if (!block) {
    uint256 hash;
    return (hash);
  }
  uint256 hashBlock = block->GetHash();
  delete block;
  return hashBlock;
//  return block->hashGenesisBlock;
}


int CBlockLocator::GetHeight()
{
  CBlockIndex* pindex = GetBlockIndex();
  if (!pindex)
    return 0;
  return pindex->nHeight;
}


CBlockIndex* CBlockLocator::GetBlockIndex()
{
  CIface *iface = GetCoinByIndex(ifaceIndex);
  blkidx_t *blockIndex = GetBlockTable(ifaceIndex); 

  // Find the first block the caller has in the main chain
  BOOST_FOREACH(const uint256& hash, vHave)
  {
    std::map<uint256, CBlockIndex*>::iterator mi = blockIndex->find(hash);
    if (mi != blockIndex->end())
    {
      CBlockIndex* pindex = (*mi).second;
      if (pindex->IsInMainChain(ifaceIndex))
        return pindex;
    }
  }

  return (GetGenesisBlockIndex(iface));
}

void CBlockLocator::Set(const CBlockIndex* pindex)
{
  CIface *iface = GetCoinByIndex(ifaceIndex);
  int nStep = 1;

  vHave.clear();
  while (pindex)
  {
    vHave.push_back(pindex->GetBlockHash());

    // Exponentially larger steps back
    for (int i = 0; pindex && i < nStep; i++)
      pindex = pindex->pprev;
    if (vHave.size() > 10)
      nStep *= 2;
  }

  CBlock *block = GetBlockByHeight(iface, 0);
  if (block) {
    uint256 hashBlock = block->GetHash();
    vHave.push_back(hashBlock);// hashGenesisBlock);
    delete block; 
  }
}

int CBlockLocator::GetDistanceBack()
{
  // Retrace how far back it was in the sender's branch
  blkidx_t *blockIndex = GetBlockTable(ifaceIndex);
  int nDistance = 0;
  int nStep = 1;
  BOOST_FOREACH(const uint256& hash, vHave)
  {
    std::map<uint256, CBlockIndex*>::iterator mi = blockIndex->find(hash);
    if (mi != blockIndex->end())
    {
      CBlockIndex* pindex = (*mi).second;
      if (pindex->IsInMainChain(ifaceIndex))
        return nDistance;
    }
    nDistance += nStep;
    if (nDistance > 10)
      nStep *= 2;
  }
  return nDistance;
}



bool CTransaction::ReadFromDisk(CTxDB& txdb, COutPoint prevout, CTxIndex& txindexRet)
{
    SetNull();
    if (!txdb.ReadTxIndex(prevout.hash, txindexRet)) {
return error(SHERR_INVAL, "CTransaction::ReadFromDisk: ReadTxIndex failure");
        return false;
}
    if (!ReadFromDisk(txindexRet.pos)) {
return error(SHERR_INVAL, "CTransaction::ReadFromDisk: ReadFromDIsk(pos) failure");
        return false;
}
    if (prevout.n >= vout.size())
    {
        SetNull();
        return false;
    }
    return true;
}









#if 0
bool CTransaction::ConnectInputs(MapPrevTx inputs, map<uint256, CTxIndex>& mapTestPool, const CDiskTxPos& posThisTx, const CBlockIndex* pindexBlock, bool fBlock, bool fMiner, bool fStrictPayToScriptHash)
{
  // Take over previous transactions' spent pointers
  // fBlock is true when this is called from AcceptBlock when a new best-block is added to the blockchain
  // fMiner is true when called from the internal usde miner
  // ... both are false when called from CTransaction::AcceptToMemoryPool
  if (!IsCoinBase())
  {
    int64 nValueIn = 0;
    int64 nFees = 0;
    for (unsigned int i = 0; i < vin.size(); i++)
    {
      COutPoint prevout = vin[i].prevout;
      assert(inputs.count(prevout.hash) > 0);
      CTxIndex& txindex = inputs[prevout.hash].first;
      CTransaction& txPrev = inputs[prevout.hash].second;

      if (prevout.n >= txPrev.vout.size() || prevout.n >= txindex.vSpent.size())
        return DoS(100, error(SHERR_INVAL, "ConnectInputs() : %s prevout.n out of range %d %d %d prev tx %s\n%s", GetHash().ToString().substr(0,10).c_str(), prevout.n, txPrev.vout.size(), txindex.vSpent.size(), prevout.hash.ToString().substr(0,10).c_str(), txPrev.ToString().c_str()));

      // If prev is coinbase, check that it's matured
      if (txPrev.IsCoinBase())
        for (const CBlockIndex* pindex = pindexBlock; pindex && pindexBlock->nHeight - pindex->nHeight < COINBASE_MATURITY; pindex = pindex->pprev)
          //if (pindex->nBlockPos == txindex.pos.nBlockPos && pindex->nFile == txindex.pos.nFile)
          if (pindex->nHeight == txindex.pos.nBlockPos)// && pindex->nFile == txindex.pos.nFile)
            return error(SHERR_INVAL, "ConnectInputs() : tried to spend coinbase at depth %d", pindexBlock->nHeight - pindex->nHeight);

      // Check for negative or overflow input values
      nValueIn += txPrev.vout[prevout.n].nValue;
      if (!MoneyRange(txPrev.vout[prevout.n].nValue) || !MoneyRange(nValueIn))
        return DoS(100, error(SHERR_INVAL, "ConnectInputs() : txin values out of range"));

    }
    // The first loop above does all the inexpensive checks.
    // Only if ALL inputs pass do we perform expensive ECDSA signature checks.
    // Helps prevent CPU exhaustion attacks.
    for (unsigned int i = 0; i < vin.size(); i++)
    {
      COutPoint prevout = vin[i].prevout;
      assert(inputs.count(prevout.hash) > 0);
      CTxIndex& txindex = inputs[prevout.hash].first;
      CTransaction& txPrev = inputs[prevout.hash].second;

      // Check for conflicts (double-spend)
      // This doesn't trigger the DoS code on purpose; if it did, it would make it easier
      // for an attacker to attempt to split the network.
      if (!txindex.vSpent[prevout.n].IsNull())
        return fMiner ? false : error(SHERR_INVAL, "ConnectInputs() : %s prev tx already used at %s", GetHash().ToString().substr(0,10).c_str(), txindex.vSpent[prevout.n].ToString().c_str());

      // Skip ECDSA signature verification when connecting blocks (fBlock=true)
      // before the last blockchain checkpoint. This is safe because block merkle hashes are
      // still computed and checked, and any change will be caught at the next checkpoint.
      if (!(fBlock && (nBestHeight < Checkpoints::GetTotalBlocksEstimate())))
      {
        // Verify signature
        if (!VerifySignature(txPrev, *this, i, fStrictPayToScriptHash, 0))
        {
          // only during transition phase for P2SH: do not invoke anti-DoS code for
          // potentially old clients relaying bad P2SH transactions
          if (fStrictPayToScriptHash && VerifySignature(txPrev, *this, i, false, 0))
            return error(SHERR_INVAL, "ConnectInputs() : %s P2SH VerifySignature failed", GetHash().ToString().substr(0,10).c_str());

          return DoS(100,error(SHERR_INVAL, "ConnectInputs() : %s VerifySignature failed", GetHash().ToString().substr(0,10).c_str()));
        }
      }

      // Mark outpoints as spent
      txindex.vSpent[prevout.n] = posThisTx;

      // Write back
      if (fBlock || fMiner)
      {
        mapTestPool[prevout.hash] = txindex;
      }
    }

    if (nValueIn < GetValueOut())
      return DoS(100, error(SHERR_INVAL, "ConnectInputs() : %s value in < value out", GetHash().ToString().substr(0,10).c_str()));

    // Tally transaction fees
    int64 nTxFee = nValueIn - GetValueOut();
    if (nTxFee < 0)
      return DoS(100, error(SHERR_INVAL, "ConnectInputs() : %s nTxFee < 0", GetHash().ToString().substr(0,10).c_str()));
    nFees += nTxFee;
    if (!MoneyRange(nFees))
      return DoS(100, error(SHERR_INVAL, "ConnectInputs() : nFees out of range"));
  }

  return true;
}
#endif



#if 0
bool CTransaction::FetchInputs(int ifaceIndex, const map<uint256, CTxIndex>& mapTestPool, bool fBlock, bool fMiner, MapPrevTx& inputsRet, bool& fInvalid)
{
  CIface *iface = GetCoinByIndex(ifaceIndex);

  if (!iface) {
    unet_log(ifaceIndex, "error obtaining coin interface.");
    return (false);
  }

  // FetchInputs can return false either because we just haven't seen some inputs
  // (in which case the transaction should be stored as an orphan)
  // or because the transaction is malformed (in which case the transaction should
  // be dropped).  If tx is definitely invalid, fInvalid will be set to true.
  fInvalid = false;

  if (IsCoinBase())
    return true; // Coinbase transactions have no inputs to fetch.

  for (unsigned int i = 0; i < vin.size(); i++)
  {
    COutPoint prevout = vin[i].prevout;
    if (inputsRet.count(prevout.hash))
      continue; // Got it already

    // Read txindex
    CTxIndex& txindex = inputsRet[prevout.hash].first;
    bool fFound = true;
    if ((fBlock || fMiner) && mapTestPool.count(prevout.hash))
    {
      // Get txindex from current proposed changes
      txindex = mapTestPool.find(prevout.hash)->second;
    }
    else
    {
      // Read txindex from txdb
      fFound = txdb.ReadTxIndex(prevout.hash, txindex);
    }
    if (!fFound && (fBlock || fMiner))
      return fMiner ? false : error(SHERR_INVAL, "FetchInputs() : %s prev tx %s index entry not found", GetHash().ToString().substr(0,10).c_str(),  prevout.hash.ToString().substr(0,10).c_str());

    // Read txPrev
    CTransaction& txPrev = inputsRet[prevout.hash].second;
    if (!fFound || txindex.pos == CDiskTxPos(1,1,1))
    {
      CTxMemPool *pool = GetTxMemPool(iface);
      // Get prev tx from single transactions in memory
      if (pool) {
        LOCK(pool->cs);
        if (!pool->exists(prevout.hash))
          return error(SHERR_INVAL, "FetchInputs() : %s mempool Tx prev not found %s", GetHash().ToString().substr(0,10).c_str(),  prevout.hash.ToString().substr(0,10).c_str());
        txPrev = pool->lookup(prevout.hash);
      }
      if (!fFound)
        txindex.vSpent.resize(txPrev.vout.size());
    }
    else
    {
      // Get prev tx from disk
      if (!txPrev.ReadFromDisk(txindex.pos))
        return error(SHERR_INVAL, "FetchInputs() : %s ReadFromDisk prev tx %s failed", GetHash().ToString().substr(0,10).c_str(),  prevout.hash.ToString().substr(0,10).c_str());
    }
  }

  // Make sure all prevout.n's are valid:
  for (unsigned int i = 0; i < vin.size(); i++)
  {
    const COutPoint prevout = vin[i].prevout;
    assert(inputsRet.count(prevout.hash) != 0);
    const CTxIndex& txindex = inputsRet[prevout.hash].first;
    const CTransaction& txPrev = inputsRet[prevout.hash].second;
    if (prevout.n >= txPrev.vout.size() || prevout.n >= txindex.vSpent.size())
    {
      // Revisit this if/when transaction replacement is implemented and allows
      // adding inputs:
      fInvalid = true;
      return DoS(100, error(SHERR_INVAL, "FetchInputs() : %s prevout.n out of range %d %d %d prev tx %s\n%s", GetHash().ToString().substr(0,10).c_str(), prevout.n, txPrev.vout.size(), txindex.vSpent.size(), prevout.hash.ToString().substr(0,10).c_str(), txPrev.ToString().c_str()));
    }
  }

  return true;
}
#endif

int64 GetBestHeight(CIface *iface)
{
  if (!iface)
    return (-1);
  return (iface->block_max);
}
void SetBestHeight(CIface *iface, int nBestHeight)
{
  if (!iface)
    return;
  if (nBestHeight < 0)
    return;
  iface->block_max = nBestHeight; 
}
int64 GetBestHeight(int ifaceIndex)
{
  return (GetBestHeight(GetCoinByIndex(ifaceIndex)));
}

bool IsInitialBlockDownload(int ifaceIndex)
{
  CBlockIndex *pindexBest = GetBestBlockIndex(ifaceIndex);

  if (pindexBest == NULL);// || nBestHeight < Checkpoints::GetTotalBlocksEstimate())
    return true;

  static int64 nLastUpdate;
  static CBlockIndex* pindexLastBest;
  if (pindexBest != pindexLastBest)
  {
    pindexLastBest = pindexBest;
    nLastUpdate = GetTime();
  }
  return (GetTime() - nLastUpdate < 15 &&
      pindexBest->GetBlockTime() < GetTime() - 24 * 60 * 60);
}

uint256 GetBestBlockChain(CIface *iface)
{
  uint256 hash;
  
  if (!iface)
    return (hash);

  iface->op_block_bestchain(iface, &hash);

  return (hash);
}

CBlockIndex *GetGenesisBlockIndex(CIface *iface) /* DEBUG: */
{
  int ifaceIndex = GetCoinIndex(iface);
  blkidx_t *blockIndex = GetBlockTable(ifaceIndex);
  if (!blockIndex)
    return (NULL);
  CBlock *block = GetBlockByHeight(iface, 0);
  if (!block)
    return (NULL);
  CBlockIndex *pindex = (*blockIndex)[block->GetHash()];
  delete block;

  return (pindex);
}

void CBlock::UpdateTime(const CBlockIndex* pindexPrev)
{
  nTime = max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());
}

bool CTransaction::IsFinal(int ifaceIndex, int nBlockHeight, int64 nBlockTime) const
{
  // Time based nLockTime implemented in 0.1.6
  if (nLockTime == 0)
    return true;
  if (nBlockHeight == 0)
    nBlockHeight = GetBestHeight(ifaceIndex);
  if (nBlockTime == 0)
    nBlockTime = GetAdjustedTime();
  if ((int64)nLockTime < ((int64)nLockTime < LOCKTIME_THRESHOLD ? (int64)nBlockHeight : nBlockTime))
    return true;
  BOOST_FOREACH(const CTxIn& txin, vin)
    if (!txin.IsFinal())
      return false;
  return true;
}


void SetBestBlockIndex(CIface *iface, CBlockIndex *pindex)
{
  if (!pindex)
    return;
  uint256 hash = pindex->GetBlockHash();
fprintf(stderr, "DEBUG: info: SetBestBlockIndex[%s]: pindex hash '%s'\n", iface->name, hash.GetHex().c_str());
  memcpy(iface->block_besthash, hash.GetRaw(), sizeof(bc_hash_t));
}
void SetBestBlockIndex(int ifaceIndex, CBlockIndex *pindex)
{
  SetBestBlockIndex(GetCoinByIndex(ifaceIndex), pindex);
}
CBlockIndex *GetBestBlockIndex(CIface *iface)
{
  int ifaceIndex = GetCoinIndex(iface);
  blkidx_t *blockIndex = GetBlockTable(ifaceIndex);
  uint256 hash;

  if (!blockIndex)
    return (NULL);

  hash.SetRaw(iface->block_besthash);
  return ((*blockIndex)[hash]);
}
CBlockIndex *GetBestBlockIndex(int ifaceIndex)
{
  return (GetBestBlockIndex(GetCoinByIndex(ifaceIndex)));
}

#if 0
bool CBlock::ConnectBlock(CTxDB& txdb, CBlockIndex* pindex)
{

  // Check it again in case a previous version let a bad block in
  if (!CheckBlock())
    return false;

  // Do not allow blocks that contain transactions which 'overwrite' older transactions,
  // unless those are already completely spent.
  // If such overwrites are allowed, coinbases and transactions depending upon those
  // can be duplicated to remove the ability to spend the first instance -- even after
  // being sent to another address.
  // See BIP30 and http://r6.ca/blog/20120206T005236Z.html for more information.
  // This logic is not necessary for memory pool transactions, as AcceptToMemoryPool
  // already refuses previously-known transaction id's entirely.
  // This rule applies to all blocks whose timestamp is after October 1, 2012, 0:00 UTC.
  int64 nBIP30SwitchTime = 1349049600;
  bool fEnforceBIP30 = (pindex->nTime > nBIP30SwitchTime);

  // BIP16 didn't become active until October 1 2012
  int64 nBIP16SwitchTime = 1349049600;
  bool fStrictPayToScriptHash = (pindex->nTime >= nBIP16SwitchTime);

  //// issue here: it doesn't know the version
  unsigned int nTxPos = pindex->nBlockPos + ::GetSerializeSize(CBlock(), SER_DISK, CLIENT_VERSION) - 1 + GetSizeOfCompactSize(vtx.size());

  map<uint256, CTxIndex> mapQueuedChanges;
  int64 nFees = 0;
  unsigned int nSigOps = 0;
  BOOST_FOREACH(CTransaction& tx, vtx)
  {
    uint256 hashTx = tx.GetHash();

    if (fEnforceBIP30) {
      CTxIndex txindexOld;
      if (txdb.ReadTxIndex(hashTx, txindexOld)) {
        BOOST_FOREACH(CDiskTxPos &pos, txindexOld.vSpent)
          if (pos.IsNull())
            return false;
      }
    }

    nSigOps += tx.GetLegacySigOpCount();
    if (nSigOps > MAX_BLOCK_SIGOPS)
      return DoS(100, error("ConnectBlock() : too many sigops"));

    CDiskTxPos posThisTx(pindex->nFile, pindex->nBlockPos, nTxPos);
    nTxPos += ::GetSerializeSize(tx, SER_DISK, CLIENT_VERSION);

    MapPrevTx mapInputs;
    if (!tx.IsCoinBase())
    {
      bool fInvalid;
      if (!tx.FetchInputs(txdb, mapQueuedChanges, true, false, mapInputs, fInvalid))
        return false;

      if (fStrictPayToScriptHash)
      {
        // Add in sigops done by pay-to-script-hash inputs;
        // this is to prevent a "rogue miner" from creating
        // an incredibly-expensive-to-validate block.
        nSigOps += tx.GetP2SHSigOpCount(mapInputs);
        if (nSigOps > MAX_BLOCK_SIGOPS)
          return DoS(100, error("ConnectBlock() : too many sigops"));
      }

      nFees += tx.GetValueIn(mapInputs)-tx.GetValueOut();

      if (!tx.ConnectInputs(mapInputs, mapQueuedChanges, posThisTx, pindex, true, false, fStrictPayToScriptHash))
        return false;
    }

    mapQueuedChanges[hashTx] = CTxIndex(posThisTx, tx.vout.size());
  }

  // Write queued txindex changes
  for (map<uint256, CTxIndex>::iterator mi = mapQueuedChanges.begin(); mi != mapQueuedChanges.end(); ++mi)
  {
    if (!txdb.UpdateTxIndex((*mi).first, (*mi).second))
      return error("ConnectBlock() : UpdateTxIndex failed");
  }

  if (vtx[0].GetValueOut() > GetBlockValue(pindex->nHeight, nFees))
    return false;

  // Update block index on disk without changing it in memory.
  // The memory index structure will be changed after the db commits.
  if (pindex->pprev)
  {
    CDiskBlockIndex blockindexPrev(pindex->pprev);
    blockindexPrev.hashNext = pindex->GetBlockHash();
    if (!txdb.WriteBlockIndex(blockindexPrev))
      return error("ConnectBlock() : WriteBlockIndex failed");
  }

  // Watch for transactions paying to me
  BOOST_FOREACH(CTransaction& tx, vtx)
    SyncWithWallets(tx, this, true);

  return true;
}
#endif

CBlock *GetBlankBlock(CIface *iface)
{
  CBlock *block;
  int err;

  if (!iface || !iface->op_block_new)
    return (NULL);

  block = NULL;
  err = iface->op_block_new(iface, &block);
  if (err) {
    int ifaceIndex = GetCoinIndex(iface);
    char errbuf[1024];

    sprintf(errbuf, "GetBlankBlock: error generating fresh block: %s [sherr %d].", sherrstr(err), err);
    unet_log(ifaceIndex, errbuf); 
  }

  return (block);
}
#if 0
CBlock *GetBlankBlock(CIface *iface)
{
  int ifaceIndex = GetCoinIndex(iface);
  CBlock *block;

  block = NULL;
  switch (ifaceIndex) {
    case SHC_COIN_IFACE:
      block = new SHCBlock();
      break;
    case USDE_COIN_IFACE:
      block = new USDEBlock();
      break;
  }

  return (block);
}
#endif

bool CBlock::ReadFromDisk(const CBlockIndex* pindex, bool fReadTransactions)
{
  CIface *iface = GetCoinByIndex(ifaceIndex);
  char errbuf[1024];
  bool ok;

  if (!pindex)
    return (false);

  ok = ReadBlock(pindex->nHeight);
  if (!ok) {
    sprintf(errbuf, "CBlock::ReadFromDisk: error obtaining block height %d (hash '%s').", pindex->nHeight, pindex->GetBlockHash().GetHex().c_str());
    return error(SHERR_INVAL, errbuf);
  }

  if (pindex->pprev && GetHash() != pindex->GetBlockHash()) {
    print();
    sprintf(errbuf, "CBlock::ReadFromDisk: block hash '%s' does not match block index '%s' for height %d.", GetHash().GetHex().c_str(), pindex->GetBlockHash().GetHex().c_str(), pindex->nHeight);

    if (pindex->nHeight > 0) {
      CIface *iface = GetCoinByIndex(ifaceIndex);
      bc_t *bc = GetBlockChain(iface);
      int err = bc_purge(bc, pindex->nHeight);
      if (err) {
        fprintf(stderr, "DEBUG: error truncating block-chain to height %d: %s\n", (pindex->nHeight-1), sherrstr(err)); 
      } else {
        fprintf(stderr, "DEBUG: truncated block-chain to height %d\n", (pindex->nHeight-1));

  SetBestBlockIndex(SHC_COIN_IFACE, pindex->pprev);
  SetBestHeight(iface, pindex->pprev->nHeight);
      }
    }

    return error(SHERR_INVAL, errbuf);
  }
  
  return (true);
}

#if 0
bool CBlock::ReadBlock(uint64_t nHeight)
{
  CIface *iface = GetCoinByIndex(ifaceIndex);
  CDataStream sBlock(SER_DISK, CLIENT_VERSION);
  size_t sBlockLen;
  unsigned char *sBlockData;
  char errbuf[1024];
  bc_t *bc;
  int err;

fprintf(stderr, "DEBUG: CBlock::ReadBlock/%s: loading height %d\n", iface->name, (int)nHeight);

  bc = GetBlockChain(iface);
  if (!bc)
    return (false);

  err = bc_get(bc, nHeight, &sBlockData, &sBlockLen);
  if (err) {
    sprintf(errbuf, "CBlock::ReadBlock[height %d]: %s (sherr %d).",
      (int)nHeight, sherrstr(err), err);
    unet_log(ifaceIndex, errbuf);
    return (false);
  }

  /* serialize binary data into block */
  sBlock.write((const char *)sBlockData, sBlockLen);
  sBlock >> *this;
  free(sBlockData);

    uint256 cur_hash = GetHash();
{
uint256 t_hash;
bc_hash_t b_hash;
memcpy(b_hash, cur_hash.GetRaw(), sizeof(bc_hash_t));
t_hash.SetRaw(b_hash);
if (!bc_hash_cmp(t_hash.GetRaw(), cur_hash.GetRaw())) {
fprintf(stderr, "DEBUG: ReadBlock: error comparing self-hash ('%s' / '%s')\n", cur_hash.GetHex().c_str(), t_hash.GetHex().c_str());
}
}
  {
    uint256 db_hash;
    bc_hash_t ret_hash;
    err = bc_get_hash(bc, nHeight, ret_hash);
    if (err) {
fprintf(stderr, "DEBUG: CBlock::ReadBlock: bc_get_hash err %d\n", err); 
      return (false);
    }
    db_hash.SetRaw((unsigned int *)ret_hash);

    if (!bc_hash_cmp(db_hash.GetRaw(), cur_hash.GetRaw())) {
fprintf(stderr, "DEBUG: CBlock::ReadBlock: hash '%s' from loaded block at pos %d has invalid hash of '%s'\n", db_hash.GetHex().c_str(), nHeight, cur_hash.GetHex().c_str());
print();
        SetNull();

  return (false);
    }
  }

  if (!CheckBlock()) {
    unet_log(ifaceIndex, "CBlock::ReadBlock: block validation failure.");
    return (false);
  }

fprintf(stderr, "DEBUG: CBlock::ReadBlock: GET retrieved pos (%d): hash '%s'\n", nHeight, GetHash().GetHex().c_str());

  return (true);
}
#endif

#if 0
bool CBlock::ReadBlock(uint64_t nHeight)
{
  switch (ifaceIndex) {
    case SHC_COIN_IFACE:
      {
        SHCBlock *block = (SHCBlock *)this;
        return (block->ReadBlock(nHeight));
      }
    case USDE_COIN_IFACE:
      {
        USDEBlock *block = (USDEBlock *)this;
        return (block->ReadBlock(nHeight));
      }
  }
  return (false);
}

bool CBlock::CheckBlock()
{
  switch (ifaceIndex) {
    case SHC_COIN_IFACE:
      {
        SHCBlock *block = (SHCBlock *)this;
        return (block->CheckBlock());
      }
    case USDE_COIN_IFACE:
      {
        USDEBlock *block = (USDEBlock *)this;
        return (block->CheckBlock());
      }
  }
  return (false);
}
#endif

bool CTransaction::CheckTransaction(int ifaceIndex) const
{
  CIface *iface = GetCoinByIndex(ifaceIndex);

  if (!iface)
    return (false);

  // Basic checks that don't depend on any context
  if (vin.empty())
    return error(SHERR_INVAL, "CTransaction::CheckTransaction() : vin empty");
  if (vout.empty())
    return error(SHERR_INVAL, "CTransaction::CheckTransaction() : vout empty");
  // Size limits
  if (::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION(iface)) > iface->max_block_size)
    return error(SHERR_INVAL, "CTransaction::CheckTransaction() : size limits failed");

  // Check for negative or overflow output values
  int64 nValueOut = 0;
  BOOST_FOREACH(const CTxOut& txout, vout)
  {
    if (txout.nValue < 0)
      return error(SHERR_INVAL, "CTransaction::CheckTransaction() : txout.nValue negative");
    if (txout.nValue > iface->max_money)
      return error(SHERR_INVAL, "CTransaction::CheckTransaction() : txout.nValue too high");
    nValueOut += txout.nValue;
    if (!MoneyRange(ifaceIndex, nValueOut))
      return error(SHERR_INVAL, "CTransaction::CheckTransaction() : txout total out of range");
  }

  // Check for duplicate inputs
  set<COutPoint> vInOutPoints;
  BOOST_FOREACH(const CTxIn& txin, vin)
  {
    if (vInOutPoints.count(txin.prevout))
      return false;
    vInOutPoints.insert(txin.prevout);
  }

  if (IsCoinBase())
  {
    if (vin[0].scriptSig.size() < 2 || vin[0].scriptSig.size() > 100)
      return error(SHERR_INVAL, "CTransaction::CheckTransaction() : coinbase script size");
  }
  else
  {
    BOOST_FOREACH(const CTxIn& txin, vin)
      if (txin.prevout.IsNull())
        return error(SHERR_INVAL, "CTransaction::CheckTransaction() : prevout is null");
  }

  return true;
}

bool CTransaction::FetchInputs(CTxDB& txdb, const map<uint256, CTxIndex>& mapTestPool, bool fBlock, bool fMiner, MapPrevTx& inputsRet, bool& fInvalid)
{
  CIface *iface = GetCoinByIndex(txdb.ifaceIndex);
  char errbuf[1024];

  // FetchInputs can return false either because we just haven't seen some inputs
  // (in which case the transaction should be stored as an orphan)
  // or because the transaction is malformed (in which case the transaction should
  // be dropped).  If tx is definitely invalid, fInvalid will be set to true.
  fInvalid = false;

  if (IsCoinBase())
    return true; // Coinbase transactions have no inputs to fetch.

  for (unsigned int i = 0; i < vin.size(); i++)
  {
    COutPoint prevout = vin[i].prevout;
    if (inputsRet.count(prevout.hash))
      continue; // Got it already

    // Read txindex
    CTxIndex& txindex = inputsRet[prevout.hash].first;
    bool fFound = true;
    if ((fBlock || fMiner) && mapTestPool.count(prevout.hash))
    {
      // Get txindex from current proposed changes
      txindex = mapTestPool.find(prevout.hash)->second;
    }
    else
    {
      // Read txindex from txdb
      fFound = txdb.ReadTxIndex(prevout.hash, txindex);
    }
    if (!fFound && (fBlock || fMiner)) {
      if (fMiner)
        return (false);
      sprintf(errbuf, "FetchInputs() : %s prev tx %s index entry not found", GetHash().ToString().substr(0,10).c_str(), prevout.hash.ToString().substr(0,10).c_str());
      return (error(SHERR_NOENT, errbuf));
    }

    // Read txPrev
    CTransaction& txPrev = inputsRet[prevout.hash].second;
    if (!fFound || txindex.pos == CDiskTxPos(0,0,0)) 
    {
      // Get prev tx from single transactions in memory
      CTxMemPool *mempool = GetTxMemPool(iface);
      {
        LOCK(mempool->cs);
        if (!mempool->exists(prevout.hash))
          return error(SHERR_INVAL, "FetchInputs() : %s mempool Tx prev not found %s", GetHash().ToString().substr(0,10).c_str(),  prevout.hash.ToString().substr(0,10).c_str());
        txPrev = mempool->lookup(prevout.hash);
      }
      if (!fFound)
        txindex.vSpent.resize(txPrev.vout.size());
    }
    else
    {
      // Get prev tx from disk
      if (!txPrev.ReadFromDisk(txindex.pos))
        return error(SHERR_INVAL, "FetchInputs() : %s ReadFromDisk prev tx %s failed", GetHash().ToString().substr(0,10).c_str(),  prevout.hash.ToString().substr(0,10).c_str());
    }
  }

  // Make sure all prevout.n's are valid:
  for (unsigned int i = 0; i < vin.size(); i++)
  {
    const COutPoint prevout = vin[i].prevout;
    assert(inputsRet.count(prevout.hash) != 0);
    const CTxIndex& txindex = inputsRet[prevout.hash].first;
    const CTransaction& txPrev = inputsRet[prevout.hash].second;
    if (prevout.n >= txPrev.vout.size() || prevout.n >= txindex.vSpent.size())
    {
      // Revisit this if/when transaction replacement is implemented and allows
      // adding inputs:
      fInvalid = true;
      return error(SHERR_INVAL, "FetchInputs() : %s prevout.n out of range %d %d %d prev tx %s\n%s", GetHash().ToString().substr(0,10).c_str(), prevout.n, txPrev.vout.size(), txindex.vSpent.size(), prevout.hash.ToString().substr(0,10).c_str(), txPrev.ToString().c_str());
    }
  }

  return true;
}

bool BlockTxExists(CIface *iface, uint256 hashTx)
{
  bc_t *bc = GetBlockTxChain(iface);
  bc_hash_t b_hash;
  int err;

  if (!bc)
    return (false);

  memcpy(b_hash, hashTx.GetRaw(), sizeof(b_hash));
  err = bc_find(bc, b_hash, NULL);
  if (err)
    return (false);

  return (true);
}

