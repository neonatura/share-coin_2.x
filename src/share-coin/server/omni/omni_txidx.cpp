
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
#include "net.h"
#include "init.h"
#include "strlcpy.h"
#include "ui_interface.h"
#include "omni_block.h"
#include "omni_txidx.h"
#include "chain.h"
#include "spring.h"

#include <boost/array.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/algorithm/string/replace.hpp>


using namespace std;




#if 0
void static BatchWriteCoins(CLevelDBBatch &batch, const uint256 &hash, const CCoins &coins) {
    if (coins.IsPruned())
        batch.Erase(make_pair('c', hash));
    else
        batch.Write(make_pair('c', hash), coins);
}

void static BatchWriteHashBestChain(CLevelDBBatch &batch, const uint256 &hash) {
    batch.Write('B', hash);
}

CCoinsViewDB::CCoinsViewDB(size_t nCacheSize, bool fMemory, bool fWipe) : db(GetDataDir() / "chainstate", nCacheSize, fMemory, fWipe) {
}

bool CCoinsViewDB::GetCoins(const uint256 &txid, CCoins &coins) { 
    return db.Read(make_pair('c', txid), coins); 
}

bool CCoinsViewDB::SetCoins(const uint256 &txid, const CCoins &coins) {
    CLevelDBBatch batch;
    BatchWriteCoins(batch, txid, coins);
    return db.WriteBatch(batch);
}

bool CCoinsViewDB::HaveCoins(const uint256 &txid) {
    return db.Exists(make_pair('c', txid)); 
}

CBlockIndex *CCoinsViewDB::GetBestBlock() {
    uint256 hashBestChain;
    if (!db.Read('B', hashBestChain))
        return NULL;
    std::map<uint256, CBlockIndex*>::iterator it = mapBlockIndex.find(hashBestChain);
    if (it == mapBlockIndex.end())
        return NULL;
    return it->second;
}

bool CCoinsViewDB::SetBestBlock(CBlockIndex *pindex) {
    CLevelDBBatch batch;
    BatchWriteHashBestChain(batch, pindex->GetBlockHash()); 
    return db.WriteBatch(batch);
}

bool CCoinsViewDB::BatchWrite(const std::map<uint256, CCoins> &mapCoins, CBlockIndex *pindex) {
    printf("Committing %u changed transactions to coin database...\n", (unsigned int)mapCoins.size());

    CLevelDBBatch batch;
    for (std::map<uint256, CCoins>::const_iterator it = mapCoins.begin(); it != mapCoins.end(); it++)
        BatchWriteCoins(batch, it->first, it->second);
    if (pindex)
        BatchWriteHashBestChain(batch, pindex->GetBlockHash());

    return db.WriteBatch(batch);
}

OMNITxDB::OMNITxDB(size_t nCacheSize, bool fMemory, bool fWipe) : CLevelDB(GetDataDir() / "blocks" / "index", nCacheSize, fMemory, fWipe) {
}

bool OMNITxDB::WriteBlockIndex(const CDiskBlockIndex& blockindex)
{
    return Write(make_pair('b', blockindex.GetBlockHash()), blockindex);
}

bool OMNITxDB::ReadBestInvalidWork(CBigNum& bnBestInvalidWork)
{
    return Read('I', bnBestInvalidWork);
}

bool OMNITxDB::WriteBestInvalidWork(const CBigNum& bnBestInvalidWork)
{
    return Write('I', bnBestInvalidWork);
}

bool OMNITxDB::WriteBlockFileInfo(int nFile, const CBlockFileInfo &info) {
    return Write(make_pair('f', nFile), info);
}

bool OMNITxDB::ReadBlockFileInfo(int nFile, CBlockFileInfo &info) {
    return Read(make_pair('f', nFile), info);
}

bool OMNITxDB::WriteLastBlockFile(int nFile) {
    return Write('l', nFile);
}

bool OMNITxDB::WriteReindexing(bool fReindexing) {
    if (fReindexing)
        return Write('R', '1');
    else
        return Erase('R');
}

bool OMNITxDB::ReadReindexing(bool &fReindexing) {
    fReindexing = Exists('R');
    return true;
}

bool OMNITxDB::ReadLastBlockFile(int &nFile) {
    return Read('l', nFile);
}

bool CCoinsViewDB::GetStats(CCoinsStats &stats) {
    leveldb::Iterator *pcursor = db.NewIterator();
    pcursor->SeekToFirst();

    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    stats.hashBlock = GetBestBlock()->GetBlockHash();
    ss << stats.hashBlock;
    int64 nTotalAmount = 0;
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        try {
            leveldb::Slice slKey = pcursor->key();
            CDataStream ssKey(slKey.data(), slKey.data()+slKey.size(), SER_DISK, CLIENT_VERSION);
            char chType;
            ssKey >> chType;
            if (chType == 'c') {
                leveldb::Slice slValue = pcursor->value();
                CDataStream ssValue(slValue.data(), slValue.data()+slValue.size(), SER_DISK, CLIENT_VERSION);
                CCoins coins;
                ssValue >> coins;
                uint256 txhash;
                ssKey >> txhash;
                ss << txhash;
                ss << VARINT(coins.nVersion);
                ss << (coins.fCoinBase ? 'c' : 'n'); 
                ss << VARINT(coins.nHeight);
                stats.nTransactions++;
                for (unsigned int i=0; i<coins.vout.size(); i++) {
                    const CTxOut &out = coins.vout[i];
                    if (!out.IsNull()) {
                        stats.nTransactionOutputs++;
                        ss << VARINT(i+1);
                        ss << out;
                        nTotalAmount += out.nValue;
                    }
                }
                stats.nSerializedSize += 32 + slValue.size();
                ss << VARINT(0);
            }
            pcursor->Next();
        } catch (std::exception &e) {
            return error("%s() : deserialize error", __PRETTY_FUNCTION__);
        }
    }
    delete pcursor;
    stats.nHeight = GetBestBlock()->nHeight;
    stats.hashSerialized = ss.GetHash();
    stats.nTotalAmount = nTotalAmount;
    return true;
}
#endif

#if 0
bool OMNITxDB::ReadTxIndex(const uint256 &txid, CDiskTxPos &pos) {
  return Read(make_pair('t', txid), pos);
}

bool OMNITxDB::WriteTxIndex(const std::vector<std::pair<uint256, CDiskTxPos> >&vect) {
  CLevelDBBatch batch;
  for (std::vector<std::pair<uint256,CDiskTxPos> >::const_iterator it=vect.begin(); it!=vect.end(); it++)
    batch.Write(make_pair('t', it->first), it->second);
  return WriteBatch(batch);
}
#endif


#if 0
bool OMNITxDB::LoadBlockIndexGuts()
{
    leveldb::Iterator *pcursor = NewIterator();

    CDataStream ssKeySet(SER_DISK, CLIENT_VERSION);
    ssKeySet << make_pair('b', uint256(0));
    pcursor->Seek(ssKeySet.str());

    // Load mapBlockIndex
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        try {
            leveldb::Slice slKey = pcursor->key();
            CDataStream ssKey(slKey.data(), slKey.data()+slKey.size(), SER_DISK, CLIENT_VERSION);
            char chType;
            ssKey >> chType;
            if (chType == 'b') {
                leveldb::Slice slValue = pcursor->value();
                CDataStream ssValue(slValue.data(), slValue.data()+slValue.size(), SER_DISK, CLIENT_VERSION);
                CDiskBlockIndex diskindex;
                ssValue >> diskindex;

                // Construct block index object
                CBlockIndex* pindexNew = InsertBlockIndex(diskindex.GetBlockHash());
                pindexNew->pprev          = InsertBlockIndex(diskindex.hashPrev);
                pindexNew->nHeight        = diskindex.nHeight;
                pindexNew->nFile          = diskindex.nFile;
                pindexNew->nDataPos       = diskindex.nDataPos;
                pindexNew->nUndoPos       = diskindex.nUndoPos;
                pindexNew->nVersion       = diskindex.nVersion;
                pindexNew->hashMerkleRoot = diskindex.hashMerkleRoot;
                pindexNew->nTime          = diskindex.nTime;
                pindexNew->nBits          = diskindex.nBits;
                pindexNew->nNonce         = diskindex.nNonce;
                pindexNew->nStatus        = diskindex.nStatus;
                pindexNew->nTx            = diskindex.nTx;

                // Watch for genesis block
                if (pindexGenesisBlock == NULL && diskindex.GetBlockHash() == hashGenesisBlock)
                    pindexGenesisBlock = pindexNew;

                if (!pindexNew->CheckIndex())
                    return error("LoadBlockIndex() : CheckIndex failed: %s", pindexNew->ToString().c_str());

                pcursor->Next();
            } else {
                break; // if shutdown requested or finished loading block index
            }
        } catch (std::exception &e) {
            return error("%s() : deserialize error", __PRETTY_FUNCTION__);
        }
    }
    delete pcursor;

    return true;
}
#endif





bool OMNITxDB::ReadDiskTx(uint256 hash, CTransaction& tx, CTxIndex& txindex)
{
  tx.SetNull();
  if (!ReadTxIndex(hash, txindex))
    return false;
  return (tx.ReadFromDisk(txindex.pos));
}

bool OMNITxDB::ReadDiskTx(uint256 hash, CTransaction& tx)
{
  CTxIndex txindex;
  return ReadDiskTx(hash, tx, txindex);
}

bool OMNITxDB::ReadDiskTx(COutPoint outpoint, CTransaction& tx, CTxIndex& txindex)
{
  return ReadDiskTx(outpoint.hash, tx, txindex);
}

bool OMNITxDB::ReadDiskTx(COutPoint outpoint, CTransaction& tx)
{
  CTxIndex txindex;
  return ReadDiskTx(outpoint.hash, tx, txindex);
}



CBlockIndex static * InsertBlockIndex(uint256 hash)
{

  if (hash == 0)
    return NULL;

  // Return existing
  blkidx_t *mapBlockIndex = GetBlockTable(OMNI_COIN_IFACE);
  map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex->find(hash);
  if (mi != mapBlockIndex->end())
    return (*mi).second;

  // Create new
  CBlockIndex* pindexNew = new CBlockIndex();
  if (!pindexNew)
    throw runtime_error("LoadBlockIndex() : new CBlockIndex failed");
  mi = mapBlockIndex->insert(make_pair(hash, pindexNew)).first;
  pindexNew->phashBlock = &((*mi).first);

  return pindexNew;
}




bool omni_FillBlockIndex()
{
  CIface *iface = GetCoinByIndex(OMNI_COIN_IFACE);
  blkidx_t *blockIndex = GetBlockTable(OMNI_COIN_IFACE);
  bc_t *bc = GetBlockChain(iface);
  CBlockIndex *lastIndex;
  OMNIBlock block;
  uint256 hash;
  int nHeight;

  bool checkBest = false;
  uint256 hashBestChain;
  OMNITxDB txdb;
  if (txdb.ReadHashBestChain(hashBestChain))
    checkBest = true;
  txdb.Close();

  int nMaxIndex = bc_idx_next(bc);

  lastIndex = NULL;
  for (nHeight = 0; nHeight < nMaxIndex; nHeight++) {
    if (0 != bc_idx_get(bc, nHeight, NULL))
      break;
    if (!block.ReadBlock(nHeight)) {
fprintf(stderr, "DEBUG: omni_FillBlockIndex: error reading block height %d in main chain.\n", nHeight);
      break;
    }
    hash = block.GetHash();

    if (nHeight == 0) {
      if (hash != omni_hashGenesisBlock) {
fprintf(stderr, "DEBUG: omni_FillBlockIndex: stopping at invalid genesis '%s' @ height %d\n", hash.GetHex().c_str(), nHeight);
        break; /* invalid genesis */
      }
    } else if (blockIndex->count(block.hashPrevBlock) == 0) {
fprintf(stderr, "DEBUG: omni_FillBlockIndex: stopping at orphan '%s' @ height %d\n", hash.GetHex().c_str(), nHeight);
      break;
    }

    CBlockIndex* pindexNew = InsertBlockIndex(blockIndex, hash);
    if (nHeight == 0) {
      OMNIBlock::pindexGenesisBlock = pindexNew;
    }
    pindexNew->pprev = lastIndex;//InsertBlockIndex(blockIndex, block.hashPrevBlock);
    if (lastIndex) lastIndex->pnext = pindexNew;
//    if (lastIndex && lastIndex->pprev == pindexNew) pindexNew->pnext = InsertBlockIndex(blockIndex, lastIndex->GetBlockHash());   

    pindexNew->nHeight        = nHeight;
    pindexNew->nVersion       = block.nVersion;
    pindexNew->hashMerkleRoot = block.hashMerkleRoot;
    pindexNew->nTime          = block.nTime;
    pindexNew->nBits          = block.nBits;
    pindexNew->nNonce         = block.nNonce;

    if (!pindexNew->CheckIndex())
      return error(SHERR_INVAL, "LoadBlockIndex() : CheckIndex failed at height %d", pindexNew->nHeight);

    if (nHeight == 0 && pindexNew->GetBlockHash() == omni_hashGenesisBlock)
      OMNIBlock::pindexGenesisBlock = pindexNew;

    lastIndex = pindexNew;

    if (checkBest && hash == hashBestChain) {
      break;
    } 
  }
  SetBestBlockIndex(iface, lastIndex);

  return true;
}

static bool hasGenesisRoot(CBlockIndex *pindexBest)
{
  CBlockIndex *pindex;

  for (pindex = pindexBest; pindex && pindex->pprev; pindex = pindex->pprev) {
    if (pindex->nHeight == 0)
      break;
  }
  if (!pindex)
    return (false);

  if (pindex->nHeight != 0 || 
      pindex->GetBlockHash() != omni_hashGenesisBlock)
    return (false);

  return (true);
}


bool OMNITxDB::LoadBlockIndex()
{
  CIface *iface = GetCoinByIndex(OMNI_COIN_IFACE);
  blkidx_t *mapBlockIndex = GetBlockTable(OMNI_COIN_IFACE);
  char errbuf[1024];

  if (!omni_FillBlockIndex())
    return (false);

  if (fRequestShutdown)
    return true;

  // Calculate bnChainWork
  vector<pair<int, CBlockIndex*> > vSortedByHeight;
  vSortedByHeight.reserve(mapBlockIndex->size());
  BOOST_FOREACH(const PAIRTYPE(uint256, CBlockIndex*)& item, (*mapBlockIndex))
  {
    CBlockIndex* pindex = item.second;
    vSortedByHeight.push_back(make_pair(pindex->nHeight, pindex));
  }
  sort(vSortedByHeight.begin(), vSortedByHeight.end());
  BOOST_FOREACH(const PAIRTYPE(int, CBlockIndex*)& item, vSortedByHeight)
  {
    CBlockIndex* pindex = item.second;
    pindex->bnChainWork = (pindex->pprev ? pindex->pprev->bnChainWork : 0) + pindex->GetBlockWork();
  }

  // Load OMNIBlock::hashBestChain pointer to end of best chain
  uint256 hashBestChain;
  if (!ReadHashBestChain(hashBestChain))
  {
    if (OMNIBlock::pindexGenesisBlock == NULL) {
      fprintf(stderr, "DEBUG: OMNITxDB::LoadBlockIndex() : OMNIBlock::hashBestChain not loaded, but pindexGenesisBlock == NULL");
      return true;
    }
    //    return error(SHERR_INVAL, "OMNITxDB::LoadBlockIndex() : OMNIBlock::hashBestChain not loaded");
  }

  CBlockIndex *pindexBest = (*mapBlockIndex)[hashBestChain];
  bool ok = true;
  if (!pindexBest)
    ok = false;
  else if (pindexBest->nHeight > 0 && !pindexBest->pprev)
    ok = false;
  else if (!hasGenesisRoot(pindexBest))
    ok = false;
  if (!ok) {
    pindexBest = GetBestBlockIndex(iface);
    if (!pindexBest)
      return error(SHERR_INVAL, "OMNITxDB::LoadBlockIndex() : OMNIBlock::hashBestChain not found in the block index");
    fprintf(stderr, "DEBUG: LoadBlockIndex: falling back to highest block height %d\n", pindexBest->nHeight);
    hashBestChain = pindexBest->GetBlockHash();
  }

  if (!pindexBest) {
    fprintf(stderr, "DEBUG: OMNITxDB::LoadBlockIndex: error: hashBestChain '%s' not found in block index table\n", (hashBestChain).GetHex().c_str());
  }

  SetBestBlockIndex(OMNI_COIN_IFACE, pindexBest);
  //  SetBestHeight(iface, pindexBest->nHeight);
  OMNIBlock::bnBestChainWork = pindexBest->bnChainWork;
  pindexBest->pnext = NULL;

  //printf("LoadBlockIndex(): OMNIBlock::hashBestChain=%s  height=%d  date=%s\n", hashBestChain.ToString().substr(0,20).c_str(), GetBestHeight(iface), DateTimeStrFormat("%x %H:%M:%S", pindexBest->GetBlockTime()).c_str());

  // Load bnBestInvalidWork, OK if it doesn't exist
  ReadBestInvalidWork(OMNIBlock::bnBestInvalidWork);

#if 0
  // Verify blocks in the best chain
  int nCheckLevel = GetArg("-checklevel", 1);
  int nCheckDepth = GetArg( "-checkblocks", 10000);
  if (nCheckDepth == 0)
    nCheckDepth = 1000000000; // suffices until the year 19000
  if (nCheckDepth > GetBestHeight(OMNI_COIN_IFACE))
    nCheckDepth = GetBestHeight(OMNI_COIN_IFACE);
  printf("Verifying last %i blocks at level %i\n", nCheckDepth, nCheckLevel);
#endif

  int nCheckDepth = (GetBestHeight(OMNI_COIN_IFACE) / 100) + 2500;
  int total = 0;
  int invalid = 0;
  int maxHeight = 0;
  int checkHeight = pindexBest->nHeight;

  CBlockIndex* pindexFork = NULL;
  map<pair<unsigned int, unsigned int>, CBlockIndex*> mapBlockPos;
  for (CBlockIndex* pindex = pindexBest; pindex && pindex->pprev; pindex = pindex->pprev)
  {
    if (fRequestShutdown || pindex->nHeight < GetBestHeight(OMNI_COIN_IFACE) - nCheckDepth)
      break;
    OMNIBlock block;
    if (!block.ReadFromDisk(pindex)) {
      fprintf(stderr, "DEBUG: OMNIBlock::LoadBlockIndex() : block.ReadFromDisk failed");
      pindexFork = pindex->pprev;
      continue;
    }
    total++;

    if (!block.CheckBlock() ||
        !block.CheckTransactionInputs(OMNI_COIN_IFACE)) {
      error (SHERR_INVAL, "(omni) LoadBlockIndex: critical: found bad block at %d, hash=%s\n", pindex->nHeight, pindex->GetBlockHash().ToString().c_str());

      pindexFork = pindex->pprev;
      invalid++;
      continue;
    }

    if (pindex->nHeight > maxHeight)
      maxHeight = pindex->nHeight;
    if (pindex->nHeight < checkHeight)
      checkHeight = pindex->nHeight;
  }
  if (pindexFork && !fRequestShutdown)
  {
    // Reorg back to the fork
    fprintf(stderr, "DEBUG: LoadBlockIndex() : *** moving best chain pointer back to block %d '%s'\n", pindexFork->nHeight, pindexFork->GetBlockHash().GetHex().c_str());
    OMNIBlock block;
    if (!block.ReadFromDisk(pindexFork))
      return error(SHERR_INVAL, "LoadBlockIndex() : block.ReadFromDisk failed");
    OMNITxDB txdb;
    block.SetBestChain(txdb, pindexFork);
    txdb.Close();

    pindexBest = pindexFork;
  }

  maxHeight++;
  sprintf(errbuf, "OMNI::LoadBlockIndex: Verified %-2.2f%% of %d total blocks: %d total invalid blocks found.", (double)(100 / (double)maxHeight * (double)total), maxHeight, invalid);
  unet_log(OMNI_COIN_IFACE, errbuf);

  CWallet *wallet = GetWallet(OMNI_COIN_IFACE);
  InitServiceWalletEvent(wallet, checkHeight);

  return true;
}


bool omni_InitBlockIndex()
{
  bool ret;

  OMNITxDB txdb("cr");
  ret = txdb.LoadBlockIndex();
  txdb.Close();
  if (!ret)
    return (false);

  if (!omni_CreateGenesisBlock())
    return (false);

  return (true);
}
   
bool OMNITxDB::WriteFlag(const std::string &name, bool fValue) 
{
  return Write(std::make_pair('F', name), fValue ? '1' : '0');
}

bool OMNITxDB::ReadFlag(const std::string &name, bool &fValue) 
{
  char ch;
  if (!Read(std::make_pair('F', name), ch))
    return false;
  fValue = ch == '1';
  return true;
}


