
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

#ifndef __OMNI__OMNI_TXIDX_H__ 
#define __OMNI__OMNI_TXIDX_H__ 


#if 0
/** CCoinsView backed by the LevelDB coin database (chainstate/) */
class CCoinsViewDB : public CCoinsView
{
protected:
    CLevelDB db;
public:
    CCoinsViewDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

    bool GetCoins(const uint256 &txid, CCoins &coins);
    bool SetCoins(const uint256 &txid, const CCoins &coins);
    bool HaveCoins(const uint256 &txid);
    CBlockIndex *GetBestBlock();
    bool SetBestBlock(CBlockIndex *pindex);
    bool BatchWrite(const std::map<uint256, CCoins> &mapCoins, CBlockIndex *pindex);
    bool GetStats(CCoinsStats &stats);
};

/** Access to the block database (blocks/index/) */
class OMNITxDB : public CLevelDB
{
public:
    OMNITxDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);
private:
    OMNITxDB(const OMNITxDB&);
    void operator=(const OMNITxDB&);
public:
    bool WriteBlockIndex(const CDiskBlockIndex& blockindex);
    bool ReadBestInvalidWork(CBigNum& bnBestInvalidWork);
    bool WriteBestInvalidWork(const CBigNum& bnBestInvalidWork);
    bool ReadBlockFileInfo(int nFile, CBlockFileInfo &fileinfo);
    bool WriteBlockFileInfo(int nFile, const CBlockFileInfo &fileinfo);
    bool ReadLastBlockFile(int &nFile);
    bool WriteLastBlockFile(int nFile);
    bool WriteReindexing(bool fReindex);
    bool ReadReindexing(bool &fReindex);
    bool ReadTxIndex(const uint256 &txid, CDiskTxPos &pos);
    bool WriteTxIndex(const std::vector<std::pair<uint256, CDiskTxPos> > &list);
    bool WriteFlag(const std::string &name, bool fValue);
    bool ReadFlag(const std::string &name, bool &fValue);
    bool LoadBlockIndexGuts();
};

#endif




class OMNITxDB : public CTxDB
{
  public:
    OMNITxDB(const char *fileMode = "r+") : CTxDB("omni_tx.dat", OMNI_COIN_IFACE, fileMode) { }
    bool ReadDiskTx(uint256 hash, CTransaction& tx, CTxIndex& txindex);
    bool ReadDiskTx(uint256 hash, CTransaction& tx);
    bool ReadDiskTx(COutPoint outpoint, CTransaction& tx, CTxIndex& txindex);
    bool ReadDiskTx(COutPoint outpoint, CTransaction& tx);
    bool LoadBlockIndex();

    bool WriteFlag(const std::string &name, bool fValue);
    bool ReadFlag(const std::string &name, bool &fValue);

  private:
    OMNITxDB(const OMNITxDB&);
    void operator=(const OMNITxDB&);
    bool LoadBlockIndexGuts();
};


static bool IsChainFile(std::string strFile)
{
    if (strFile == "omni_tx.dat")
        return true;

    return false;
}

bool omni_InitBlockIndex();



#endif /** __OMNI__OMNI_TXIDX_H__ */
