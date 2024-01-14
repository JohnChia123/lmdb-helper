// lmdbhelper.h
#include <lmdb.h>
#include <string>
#include <vector>

typedef std::vector<uint8_t> MemBuffer;

class lmdbhelper {
private:
    MDB_env* dbenv;

public:
    lmdbhelper(std::string& dbfile);
    ~lmdbhelper();

    int setValue(std::string& tableName, std::string& key, MemBuffer& value);
    int getValue(std::string& tableName, std::string& key, MemBuffer& value);
    int beginTxn(MDB_txn** txn, unsigned int flags);
    void closeDbi(MDB_dbi& dbi);
    uint32_t clearTableDataKey(std::string& tableName, std::string& key);
    uint32_t setKeyTimestamp(std::string& tableName, std::string& key);
    uint32_t getKeyTimestamp(std::string& tableName, std::string& key);
    std::vector<MemBuffer> getAllKeyValues(std::string& tableName, std::string& key);
};

