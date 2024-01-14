#include <iostream>
#include <iomanip>
#include <cstring>
#include <lmdb.h>
#include <string>
#include <vector>
#include "lmdbhelper.h"
#include <getopt.h>

lmdbhelper::lmdbhelper(std::string& dbfile) {
        dbenv = nullptr; 
        int res = mdb_env_create(&dbenv);
        if (res != MDB_SUCCESS)
            throw std::runtime_error("Failed to create LMDB environment.");

        res = mdb_env_set_mapsize(dbenv, 10 * 1024 * 1024);
        if (res != MDB_SUCCESS)
            throw std::runtime_error("Failed to set LMDB map size.");

        res = mdb_env_set_maxdbs(dbenv, 10);
        if (res != MDB_SUCCESS)
            throw std::runtime_error("Failed to set LMDB maximum number of databases.");

        res = mdb_env_open(dbenv, dbfile.c_str(), MDB_NOSUBDIR | MDB_WRITEMAP, 0644);
        if (res != MDB_SUCCESS)
            throw std::runtime_error("Failed to open LMDB environment.");
    }

lmdbhelper::~lmdbhelper() {
        if (dbenv != nullptr)
            mdb_env_close(dbenv);
    }

int lmdbhelper::getValue(std::string& tableName, std::string& key, MemBuffer& value) {
        MDB_txn* txn = nullptr;
        MDB_dbi dbi;
        int res = mdb_txn_begin(dbenv, nullptr, MDB_DUPSORT, &txn);
        if (res != MDB_SUCCESS)
            return res;

        res = mdb_dbi_open(txn, tableName.c_str(), MDB_DUPSORT, &dbi);
        if (res != MDB_SUCCESS) {
            mdb_txn_abort(txn);
            return res;
        }

        MDB_val mdbKey, mdbValue;
        mdbKey.mv_size = key.size();
        mdbKey.mv_data = const_cast<char*>(key.data());
        res = mdb_get(txn, dbi, &mdbKey, &mdbValue);
        if (res == MDB_SUCCESS) {
            value.assign(static_cast<uint8_t*>(mdbValue.mv_data), static_cast<uint8_t*>(mdbValue.mv_data) + mdbValue.mv_size);
        }

        mdb_txn_abort(txn);
        mdb_dbi_close(dbenv, dbi);
        return res;
    }


int lmdbhelper::setValue(std::string& tableName, std::string& key, MemBuffer& value) {
        //std::cout << "Entering setValue..." << std::endl;
        MDB_txn* txn = nullptr;
        MDB_dbi dbi;

        //std::cout << "Starting transaction..." << std::endl;
        int res = mdb_txn_begin(dbenv, nullptr, MDB_DUPSORT, &txn);
        if (res != MDB_SUCCESS) {
            std::cerr << "Failed to start transaction. Error: " << mdb_strerror(res) << std::endl;
            return res;
        }  

        //std::cout << "Opening DBI..." << std::endl;

        res = mdb_dbi_open(txn, tableName.c_str(), MDB_CREATE | MDB_DUPSORT, &dbi);
        if (res != MDB_SUCCESS) {
            std::cerr << "Failed to open DBI. Error: " << mdb_strerror(res) << std::endl;
            mdb_txn_abort(txn);
            return res;
        }

        MDB_val mdbKey, mdbValue;
        mdbKey.mv_size = key.size();
        mdbKey.mv_data = const_cast<char*>(key.data());
        mdbValue.mv_size = value.size();
        mdbValue.mv_data = value.data();

        //std::cout << "Putting data into LMDB..." << std::endl;
        res = mdb_put(txn, dbi, &mdbKey, &mdbValue, 0); //last parameter was originally 0 

        if (res == MDB_SUCCESS) {
            //std::cout << "Data inserted successfully. Committing transaction..." << std::endl;
            mdb_txn_commit(txn);
        } else {
            std::cerr << "Failed to insert data. Error: " << mdb_strerror(res) << std::endl;
            mdb_txn_abort(txn);
        }

        mdb_dbi_close(dbenv, dbi);
        return res;
    }
int lmdbhelper::beginTxn(MDB_txn** txn, unsigned int flags) {
    return mdb_txn_begin(dbenv, NULL, flags, txn);
}

void lmdbhelper::closeDbi(MDB_dbi& dbi) {
    mdb_dbi_close(dbenv, dbi);
}

uint32_t lmdbhelper::clearTableDataKey(std::string& tableName, std::string& key) {
    MDB_txn* txn;
    MDB_dbi dbi;
    beginTxn(&txn, MDB_DUPSORT);  // Flags can be changed if needed
    mdb_dbi_open(txn, tableName.c_str(), MDB_CREATE | MDB_DUPSORT, &dbi);
    MDB_val keyVal;
    keyVal.mv_size = key.size();
    keyVal.mv_data = (void*)key.c_str();
    int rc = mdb_del(txn, dbi, &keyVal, NULL);
    mdb_txn_commit(txn);
    closeDbi(dbi);
    return rc;
}

uint32_t lmdbhelper::setKeyTimestamp(std::string& tableName, std::string& key) {
    uint32_t timestamp = (uint32_t)time(NULL);  // Get the current Unix timestamp
    //std::string key = "config-ts";
    MemBuffer value(reinterpret_cast<uint8_t*>(&timestamp), reinterpret_cast<uint8_t*>(&timestamp) + sizeof(timestamp));
    return setValue(tableName, key, value);
}

uint32_t lmdbhelper::getKeyTimestamp(std::string& tableName, std::string& key) {
    //std::string key = "config-ts";
    MemBuffer valueBuffer;
    if (getValue(tableName, key, valueBuffer) == 0 && valueBuffer.size() == sizeof(uint32_t)) {
        // Convert MemBuffer back to uint32_t
        return *reinterpret_cast<uint32_t*>(valueBuffer.data());
    }
    return 0;  // Return 0 or some default/error value if fetching fails
}
std::vector<MemBuffer> lmdbhelper::getAllKeyValues(std::string& tableName, std::string& key) {
    std::vector<MemBuffer> results;
    MDB_txn * txn;
    MDB_dbi dbi;
    MDB_cursor* cursor;

    int res = beginTxn(&txn, MDB_RDONLY | MDB_DUPSORT);

    if(res != MDB_SUCCESS) {
        //Handle the error
        return results;
    }

    res = mdb_dbi_open(txn, tableName.c_str(), MDB_DUPSORT, &dbi);
    if(res != MDB_SUCCESS) {
        mdb_txn_abort(txn);
        return results;
    }

    mdb_cursor_open(txn, dbi, &cursor);

    MDB_val mdbKey, mdbValue;
    mdbKey.mv_size = key.size();
    mdbKey.mv_data = const_cast<char*>(key.c_str());

    res = mdb_cursor_get(cursor, &mdbKey, &mdbValue, MDB_SET_KEY);
    while (res == MDB_SUCCESS) {
        // Convert mdbValue.mv_data to MemBuffer and add to results
        MemBuffer buffer(static_cast<uint8_t*>(mdbValue.mv_data), static_cast<uint8_t*>(mdbValue.mv_data) + mdbValue.mv_size);
        results.push_back(buffer);
        res = mdb_cursor_get(cursor, &mdbKey, &mdbValue, MDB_NEXT_DUP);
    }   
    
    mdb_cursor_close(cursor);
    mdb_txn_abort(txn);
    closeDbi(dbi);

    return results;
}


//int main() {
//    std::cout << "Hello world!" << std::endl;
//
//    std::string LMDBPATH = "/home/PI/pidata.db";
//    std::string data = "data";
//
//    try {
//        lmdbhelper dbhelper(LMDBPATH);
//
//        // Write some sample values
//        struct kv {
//            char* key;
//            char* value;
//        };
//        struct kv kvlist[] = {
//            { "PME.Engine Speed", "200.24" },
//            { "SME.Engine Speed", "34480.098" },
//        };
//        for (unsigned int idx = 0; idx < sizeof(kvlist) / sizeof(kvlist[0]); ++idx) {
//            struct kv* kvptr = &kvlist[idx];
//            std::cout << "Setting: " << kvptr->key << " <- " << kvptr->value << std::endl;
//
//            MemBuffer value(kvptr->value, kvptr->value + std::strlen(kvptr->value));
//            std::string theKey = kvptr->key;
//            int res = dbhelper.setValue(data , theKey, value);
//            if (res != MDB_SUCCESS)
//                std::cout << "Failed to set value. Error code: " << res << std::endl;
//        }
//
//        // Read all values from the db
//        {
//            std::string tableName = "data";
//            std::vector<std::string> keyList = {
//                "PME.Engine Speed",
//                "SME.Engine Speed",
//                "CME.Engine Load"
//            };
//
//            std::cout << "Reading from keys..." << std::endl;
//            for (unsigned int idx = 0; idx < keyList.size(); ++idx) {
//                std::string& key = keyList[idx];
//                MemBuffer value;
//                int res = dbhelper.getValue(tableName, key, value);
//                if (res == MDB_SUCCESS) {
//                    std::string valueStr(value.begin(), value.end());
//                    std::cout << "  " << std::setw(2) << std::setfill('0') << idx + 1 << ") '" << key << "': " << valueStr << std::endl;
//                }
//                else if (res == MDB_NOTFOUND) {
//                    std::cout << "  " << std::setw(2) << std::setfill('0') << idx + 1 << ") '" << key << "': *** not found!" << std::endl;
//                }
//                else {
//                    std::cout << "  " << std::setw(2) << std::setfill('0') << idx + 1 << ") '" << key << "': error " << res << std::endl;
//                }
//            }
//        }
//    }
//    catch (const std::exception& e) {
//        std::cout << "Exception occurred: " << e.what() << std::endl;
//    }
//
//    return 0;
//}
//
