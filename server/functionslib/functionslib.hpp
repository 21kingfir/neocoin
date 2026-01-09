#ifndef FUNCTIONSLIB_HPP
#define FUNCTIONSLIB_HPP

#include <iostream>
#include <sqlite3.h>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <nlohmann/json.hpp>
#include <httplib.h>
#include <mutex>
#include <cryptopp/rsa.h>
#include <cryptopp/base64.h>
#include <cryptopp/queue.h>
#include <cryptopp/osrng.h>
#include <cryptopp/files.h>
#include <handlerlib/handlerlib.hpp>
#include <functionslib/functionslib.hpp>
#include <atomic>

using namespace functionslib;
using namespace std;
using namespace httplib;
using namespace CryptoPP;
using namespace chrono;

using json = nlohmann::json;


namespace functionslib {

    uint getendid(sqlite3* database, const char* recherche);

    uint getusersold(string id, sqlite3* db, const char* getsoldsql);

    string rsa_decrypt_b64(const string& cipher_b64, RSA::PrivateKey& priv);

    int uncrypttx(tx* encryptedtx, uint keys_id, const char* sqlrequest, sqlite3* database);
}

#endif