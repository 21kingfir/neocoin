#include <functionslib.hpp>

uint getendid(sqlite3* database, const char* recherche) {
    sqlite3_stmt* stmt;
    int request = sqlite3_prepare_v2(database, recherche, -1, &stmt, nullptr);

    uint result;

    if (request != SQLITE_DONE) {
        return 1;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return result;
}

uint getusersold(string id, sqlite3* db, const char* getsoldsql) {
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, getsoldsql, -1, &stmt, nullptr);

    if (rc != SQLITE_DONE) {
        return 0;
    }
    uint sold;

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, nullptr);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        sold += sqlite3_column_int(stmt, 3);
    }
    return sold;
}

string rsa_decrypt_b64(const string& cipher_b64, RSA::PrivateKey& priv) {
    AutoSeededRandomPool rng;
    string cipher, plain;

    StringSource ss1(cipher_b64, true, new Base64Decoder(new StringSink(cipher)));

    RSAES_OAEP_SHA256_Decryptor dec(priv);
    StringSource ss2(cipher, true, new PK_DecryptorFilter(rng, dec, new StringSink(plain)));

    return plain;
}

int uncrypttx(tx* encryptedtx, uint keys_id, const char* sqlrequest, sqlite3* database) {
    sqlite3_stmt* stmt;
    int request = sqlite3_prepare_v2(database, sqlrequest, -1, &stmt, nullptr);

    if (request != SQLITE_DONE) {
        return 1;
    };

    sqlite3_bind_int(stmt, 1, keys_id);

    int exec = sqlite3_step(stmt);

    auto rawstring = sqlite3_column_text(stmt, 0);
    string privkey = rawstring ? reinterpret_cast<const char*>(rawstring) : "";
    privkey.erase(0, 28);
    privkey.erase(privkey.end()-26, privkey.end());
    
    string derprivkey;
    StringSource ss(privkey, true, new Base64Decoder(new StringSink(derprivkey)));

    RSA::PrivateKey privatekey;
    StringSource fprivatekey(derprivkey, true);
    privatekey.Load(fprivatekey);

    sqlite3_finalize(stmt);

    if (exec != SQLITE_OK) {
        return 1;
    }

    encryptedtx->ids = rsa_decrypt_b64(encryptedtx->ids, privatekey);
    encryptedtx->idr = rsa_decrypt_b64(encryptedtx->idr, privatekey);
    encryptedtx->amount = rsa_decrypt_b64(encryptedtx->amount, privatekey);
    encryptedtx->age = rsa_decrypt_b64(encryptedtx->age, privatekey);
    return 0;
}