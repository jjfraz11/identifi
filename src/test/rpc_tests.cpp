#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/test/unit_test.hpp>
#include <ctime>

#include "main.h"
#include "base58.h"
#include "util.h"
#include "identifirpc.h"

using namespace std;
using namespace json_spirit;
extern bool shutdownRequested;

BOOST_AUTO_TEST_SUITE(rpc_tests)

int dbNumber = 0;

static Value CallRPC(string args)
{
    vector<string> vArgs;
    boost::split(vArgs, args, boost::is_any_of(" \t"));
    string strMethod = vArgs[0];
    vArgs.erase(vArgs.begin());
    Array params = RPCConvertValues(strMethod, vArgs);

    rpcfn_type method = tableRPC[strMethod]->actor;
    try {
        Value result = (*method)(params, false);
        return result;
    }
    catch (Object& objError)
    {
        throw runtime_error(find_value(objError, "message").get_str());
    }
}

void resetDB() {
    shutdownRequested = true;
    delete pidentifidb;
    pidentifidb = new CIdentifiDB(100, GetDataDir() / to_string(dbNumber));
    dbNumber++;
    shutdownRequested = false;
}

BOOST_AUTO_TEST_CASE(save_and_read_msgs)
{
    Value r;

    BOOST_CHECK_NO_THROW(r=CallRPC("getmsgcount"));
    BOOST_CHECK_EQUAL(r.get_int(), 3);

    BOOST_CHECK_NO_THROW(r=CallRPC("getidentifiercount"));
    BOOST_CHECK_EQUAL(r.get_int(), 8);

    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email alice@example.com email bob@example.com 1 positive"));
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email bob@example.com email carl@example.com 1 positive"));
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email carl@example.com email david@example.com 1 positive"));
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email david@example.com email bob@example.com 1 positive"));
    
    BOOST_CHECK_NO_THROW(r=CallRPC("rate email elena@example.com 1 positive"));

    BOOST_CHECK_NO_THROW(r=CallRPC("getmsgcount"));
    BOOST_CHECK_EQUAL(r.get_int(), 8);

    BOOST_CHECK_NO_THROW(r=CallRPC("getidentifiercount"));
    BOOST_CHECK_EQUAL(r.get_int(), 13);

    BOOST_CHECK_NO_THROW(r=CallRPC("getmsgsbyauthor email alice@example.com"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 1);
    Object firstMessage, data, signedData;
    BOOST_CHECK_NO_THROW(firstMessage = r.get_array().front().get_obj());
    BOOST_CHECK_NO_THROW(data=find_value(firstMessage, "data").get_obj());
    BOOST_CHECK_NO_THROW(signedData=find_value(data, "signedData").get_obj());
    BOOST_CHECK_NO_THROW(find_value(signedData, "timestamp").get_int());
    BOOST_CHECK(!find_value(signedData, "author").get_array().empty());
    BOOST_CHECK(!find_value(signedData, "recipient").get_array().empty());
    BOOST_CHECK(find_value(signedData, "comment").get_str().size() > 0);
    BOOST_CHECK(!find_value(data, "signature").get_obj().empty());
    BOOST_CHECK_EQUAL(find_value(firstMessage, "published").get_bool(), true);

    BOOST_CHECK_NO_THROW(r=CallRPC("getmsgsbyrecipient email bob@example.com"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 2);
    BOOST_CHECK_NO_THROW(firstMessage = r.get_array().front().get_obj());
    BOOST_CHECK_NO_THROW(data=find_value(firstMessage, "data").get_obj());
    BOOST_CHECK_NO_THROW(signedData=find_value(data, "signedData").get_obj());
    BOOST_CHECK_NO_THROW(find_value(signedData, "timestamp").get_int());
    BOOST_CHECK(!find_value(signedData, "author").get_array().empty());
    BOOST_CHECK(!find_value(signedData, "recipient").get_array().empty());
    BOOST_CHECK(find_value(signedData, "comment").get_str().size() > 0);
    BOOST_CHECK(!find_value(data, "signature").get_obj().empty());
    BOOST_CHECK(find_value(firstMessage, "hash").get_str().size() > 0);
    BOOST_CHECK_EQUAL(find_value(firstMessage, "published").get_bool(), true);

    BOOST_CHECK_NO_THROW(r=CallRPC("getmsgsbyrecipient email bob@example.com 20 0 email alice@example.com 3 rating"));

    BOOST_CHECK_NO_THROW(r=CallRPC("savemsgfromdata {\"signedData\":{\"timestamp\":1234567,\"author\":[[\"mbox\",\"mailto:alice@example.com\"],[\"profile\",\"http://www.example.com/alice\"]],\"recipient\":[[\"mbox\",\"mailto:bob@example.com\"],[\"profile\",\"http://www.example.com/bob\"]],\"type\":\"review\",\"comment\":\"thanks\",\"rating\":100,\"minRating\":-100,\"maxRating\":100},\"signature\":{}} false"));
    BOOST_CHECK_EQUAL(r.get_str(), "H3EpyBikTvEJwffX5kj3FaDBL4Lub3ZzJz5JAGuYzRCs");
    BOOST_CHECK_NO_THROW(r=CallRPC("getidentifiercount"));
    BOOST_CHECK_EQUAL(r.get_int(), 17);

    BOOST_CHECK_NO_THROW(r=CallRPC("getmsgbyhash H3EpyBikTvEJwffX5kj3FaDBL4Lub3ZzJz5JAGuYzRCs"));
    BOOST_CHECK_EQUAL(find_value(r.get_array().front().get_obj(), "published").get_bool(), false);
    BOOST_CHECK_NO_THROW(CallRPC("publish H3EpyBikTvEJwffX5kj3FaDBL4Lub3ZzJz5JAGuYzRCs"));
    BOOST_CHECK_NO_THROW(r=CallRPC("getmsgbyhash H3EpyBikTvEJwffX5kj3FaDBL4Lub3ZzJz5JAGuYzRCs"));
    BOOST_CHECK_EQUAL(find_value(r.get_array().front().get_obj(), "published").get_bool(), true);

    BOOST_CHECK_NO_THROW(r=CallRPC("getmsgbyhash asdf"));
    BOOST_CHECK(r.get_array().empty());

    BOOST_CHECK_NO_THROW(r=CallRPC("getmsgsbyauthor profile http://www.example.com/alice"));
    firstMessage = r.get_array().front().get_obj();
    data=find_value(firstMessage, "data").get_obj();
    BOOST_CHECK(!find_value(data, "signature").get_obj().empty());

    BOOST_CHECK_NO_THROW(r=CallRPC("getmsgsbyauthor profile http://www.example.com/alice 20 0 email alice@example.com 3 review"));

    BOOST_CHECK_THROW(r=CallRPC("addsignature H3EpyBikTvEJwffX5kj3FaDBL4Lub3ZzJz5JAGuYzRCs QuFEJZLioVcvzrGjfdm2QFsV7Nrmm8vdDMCmW9X2xgpZpYPKrTkZzXjQNjcvfjuu7GrxQKGiUZjXznLkULYjet3V invalidsignature"), runtime_error);
    // TODO: add correct signature: BOOST_CHECK_NO_THROW(r=CallRPC("addsignature H3EpyBikTvEJwffX5kj3FaDBL4Lub3ZzJz5JAGuYzRCs QuFEJZLioVcvzrGjfdm2QFsV7Nrmm8vdDMCmW9X2xgpZpYPKrTkZzXjQNjcvfjuu7GrxQKGiUZjXznLkULYjet3V AN1rKvt2BThjKHbzpNrgxrfRrssN3Fq6byGGaQL3GjjKsqnYgaCJmQqh5Pj5fDjd7UNTHTSY21Xncpe2NigZ2sZFR57brJnJZ"));

    BOOST_CHECK_NO_THROW(r=CallRPC("getmsgsbyauthor profile http://www.example.com/alice"));
    firstMessage = r.get_array().front().get_obj();
    data=find_value(firstMessage, "data").get_obj();
    BOOST_CHECK(!find_value(data, "signature").get_obj().empty());

    BOOST_CHECK_NO_THROW(r=CallRPC("getmsgsafter 0"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 9);

    BOOST_CHECK_NO_THROW(r=CallRPC("getmsgsafter 0 1 20 email alice@example.com 3 review"));

    BOOST_CHECK_NO_THROW(r=CallRPC("getlatestmsgs"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 9);

    BOOST_CHECK_NO_THROW(r=CallRPC("getlatestmsgs 20 0 email alice@example.com 0 review"));

    BOOST_CHECK_EQUAL(CallRPC("getmsgcount").get_int(), 9);
    BOOST_CHECK_NO_THROW(r=CallRPC("deletemsg H3EpyBikTvEJwffX5kj3FaDBL4Lub3ZzJz5JAGuYzRCs"));
    BOOST_CHECK_EQUAL(CallRPC("getmsgcount").get_int(), 8);
}

BOOST_AUTO_TEST_CASE(connections) {
    resetDB();
    Value r;
    Object data;
    BOOST_CHECK_NO_THROW(r=CallRPC("saveconnection email bob@example.com email alice@example.com name Alice"));
    BOOST_CHECK_NO_THROW(r=CallRPC("getname email alice@example.com"));

    BOOST_CHECK_NO_THROW(r=CallRPC("saveconnection email alice@example.com email bob@example.com email bob@example.org"));
    BOOST_CHECK_NO_THROW(r=CallRPC("refuteconnection email alice@example.com email bob@example.com email bob@example.org"));

    BOOST_CHECK_NO_THROW(r=CallRPC("search alice"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 2);

    BOOST_CHECK_NO_THROW(r=CallRPC("getconnections email alice@example.com"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 1);

    BOOST_CHECK_NO_THROW(r=CallRPC("getconnections email alice@example.com 20 0 email alice@example.com 3"));
}

BOOST_AUTO_TEST_CASE(overview) {
    resetDB();
    Value r;
    Object data;

    // only the latest msg from the same author to the same recipient should be taken into account
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email alice@example.com email carl@example.com 1"));
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email alice@example.com email dean@example.com 1"));
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email alice@example.com email bob@example.com -1"));
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email alice@example.com email bob@example.com 0"));
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email alice@example.com email bob@example.com 1"));

    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email bob@example.com email alice@example.com 1"));
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email bob@example.com email alice@example.com 0"));
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email bob@example.com email alice@example.com -1"));

    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email carl@example.com email alice@example.com 1"));
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email dean@example.com email alice@example.com 0"));

    BOOST_CHECK_NO_THROW(r=CallRPC("overview email alice@example.com"));
    data=r.get_obj();
    BOOST_CHECK_EQUAL(find_value(data, "authoredPositive").get_int(), 3);
    BOOST_CHECK_EQUAL(find_value(data, "authoredNeutral").get_int(), 0);
    BOOST_CHECK_EQUAL(find_value(data, "authoredNegative").get_int(), 0);
    BOOST_CHECK_EQUAL(find_value(data, "receivedPositive").get_int(), 1);
    BOOST_CHECK_EQUAL(find_value(data, "receivedNeutral").get_int(), 1);
    BOOST_CHECK_EQUAL(find_value(data, "receivedNegative").get_int(), 1);
    BOOST_CHECK_EQUAL(find_value(data, "trustMapSize").get_int(), 3);

    BOOST_CHECK_NO_THROW(r=CallRPC("overview email alice@example.com email alice@example.com 3"));
    data=r.get_obj();
    BOOST_CHECK_EQUAL(find_value(data, "authoredPositive").get_int(), 3);
    BOOST_CHECK_EQUAL(find_value(data, "authoredNeutral").get_int(), 0);
    BOOST_CHECK_EQUAL(find_value(data, "authoredNegative").get_int(), 0);
    BOOST_CHECK_EQUAL(find_value(data, "receivedPositive").get_int(), 1);
    BOOST_CHECK_EQUAL(find_value(data, "receivedNeutral").get_int(), 1);
    BOOST_CHECK_EQUAL(find_value(data, "receivedNegative").get_int(), 1);
}

BOOST_AUTO_TEST_CASE(keys_and_signatures) {
    Value r;
    BOOST_CHECK_NO_THROW(r=CallRPC("getidentifiercount"));
    int identifierCountBefore = r.get_int();
    BOOST_CHECK_NO_THROW(r=CallRPC("listmykeys"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 1);
    BOOST_CHECK_THROW(r=CallRPC("importprivkey invalid_key"), runtime_error);
    BOOST_CHECK_NO_THROW(r=CallRPC("importprivkey 5K1T7u3NA55ypnDDBHB61MZ2hFxCoNbBeZj5dhQttPJFKo85MfR"));
    BOOST_CHECK_EQUAL(r.get_bool(), true);
    BOOST_CHECK_NO_THROW(r=CallRPC("setdefaultkey 5K1T7u3NA55ypnDDBHB61MZ2hFxCoNbBeZj5dhQttPJFKo85MfR"));
    BOOST_CHECK_EQUAL(r.get_bool(), true);
    BOOST_CHECK_NO_THROW(r=CallRPC("getnewkey"));
    BOOST_CHECK_NO_THROW(r=CallRPC("listmykeys"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 3);
    BOOST_CHECK_NO_THROW(r=CallRPC("getidentifiercount"));
    BOOST_CHECK_EQUAL(r.get_int(), identifierCountBefore + 4);
}

BOOST_AUTO_TEST_CASE(trust_paths) {
    resetDB();
    Value r;
    Object firstMessage;
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email abc@example.com email def@example.com -1"));
    string msgHash = r.get_str();
    BOOST_CHECK_NO_THROW(r=CallRPC("getpath email abc@example.com email def@example.com"));
    BOOST_CHECK(r.get_array().empty());
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email abc@example.com email def@example.com 1"));
    msgHash = r.get_str();
    BOOST_CHECK_NO_THROW(r=CallRPC("gettruststep email abc@example.com email def@example.com"));
    BOOST_CHECK_EQUAL(r.get_str(), msgHash);
    BOOST_CHECK_NO_THROW(r=CallRPC("getpath email abc@example.com email def@example.com"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 1);
    BOOST_CHECK_NO_THROW(r=CallRPC(string("deletemsg ") + msgHash));
    BOOST_CHECK_EQUAL(CallRPC("gettruststep email abc@example.com email def@example.com").get_str().size(), 0);
    BOOST_CHECK_NO_THROW(r=CallRPC("getpath email abc@example.com email def@example.com"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 0);
    
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email abc@example.com email def@example.com 1"));
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email cba@example.com email def@example.com 1"));
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email def@example.com email fed@example.com 1"));
    msgHash = r.get_str();
    BOOST_CHECK_NO_THROW(r=CallRPC("getpath email abc@example.com email fed@example.com"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 2);
    BOOST_CHECK_NO_THROW(r=CallRPC("getpath email cba@example.com email fed@example.com"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 2);
    BOOST_CHECK_NO_THROW(r=CallRPC(string("deletemsg ") + msgHash));
    BOOST_CHECK_EQUAL(CallRPC("gettruststep email abc@example.com email fed@example.com").get_str().size(), 0);
    BOOST_CHECK_NO_THROW(r=CallRPC("getpath email abc@example.com email fed@example.com"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 0);
    BOOST_CHECK_EQUAL(CallRPC("gettruststep email cba@example.com email fed@example.com").get_str().size(), 0);
    BOOST_CHECK_NO_THROW(r=CallRPC("getpath email cba@example.com email fed@example.com"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 0);

    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email abc@example.com email def@example.com 1"));
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email cba@example.com email def@example.com 1"));
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email def@example.com email fed@example.com 1"));
    msgHash = r.get_str();
    BOOST_CHECK_NO_THROW(r=CallRPC("getpath email abc@example.com email fed@example.com"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 2);
    BOOST_CHECK_NO_THROW(r=CallRPC("getpath email cba@example.com email fed@example.com"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 2);
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email def@example.com email fed@example.com -1"));
    BOOST_CHECK_NO_THROW(r=CallRPC("getpath email abc@example.com email fed@example.com"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 0);
    BOOST_CHECK_NO_THROW(r=CallRPC("getpath email cba@example.com email fed@example.com"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 0);
    BOOST_CHECK_NO_THROW(r=CallRPC("getpath email def@example.com email fed@example.com"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 0);

    BOOST_CHECK_NO_THROW(r=CallRPC("setdefaultkey 5K1T7u3NA55ypnDDBHB61MZ2hFxCoNbBeZj5dhQttPJFKo85MfR"));
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating keyID 1Jzbz2SsqnFpSrADASRywQEwZGZEY6y3As keyID 1CevLPhmqURncVPniRtGVAFzu4dM6KMwRr 5"));
    msgHash = r.get_str();
    BOOST_CHECK_NO_THROW(r=CallRPC("savemsgfromdata {\"signature\":{\"pubKey\":\"RXfBZLerFkiD9k3LgreFbiGEyNFjxRc61YxAdPtHPy7HpDDxBQB62UBJLDniZwxXcf849WSra1u6TDCvUtdJxFJU\",\"signature\":\"AN1rKvthPcmPwv2haGvpG2BgFNzDPNJ9FuvTN5AZT3NjKtdgrzrDP88pVSKPs4sJc4w1n5Fbig7SWnucvWsG57gy7U5ZSuTBg\"},\"signedData\":{\"author\":[[\"keyID\",\"1CevLPhmqURncVPniRtGVAFzu4dM6KMwRr\"]],\"comment\":\"trusted\",\"maxRating\":10,\"minRating\":-10,\"rating\":1,\"recipient\":[[\"email\",\"james@example.com\"]],\"timestamp\":1392476848,\"type\":\"review\"}}"));
    BOOST_CHECK_EQUAL(r.get_str(), "HHAMaLsfAgFFWTzXaoug4JaZ6UPFkuemwTpodmq7pEhg");
    BOOST_CHECK_NO_THROW(r=CallRPC("getmsgsbyauthor keyID 1Jzbz2SsqnFpSrADASRywQEwZGZEY6y3As"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 1);
    BOOST_CHECK_NO_THROW(r=CallRPC("getmsgsbyauthor keyID 1CevLPhmqURncVPniRtGVAFzu4dM6KMwRr"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 1);
    firstMessage = r.get_array().front().get_obj();
    BOOST_CHECK_EQUAL(find_value(firstMessage, "priority").get_int(), 50);
    CallRPC("deletemsg " + msgHash);
    BOOST_CHECK_NO_THROW(r=CallRPC("getmsgsbyauthor keyID 1CevLPhmqURncVPniRtGVAFzu4dM6KMwRr"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 1);
    firstMessage = r.get_array().front().get_obj();
    BOOST_CHECK_EQUAL(find_value(firstMessage, "priority").get_int(), 0);
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating keyID 1Jzbz2SsqnFpSrADASRywQEwZGZEY6y3As keyID 1CevLPhmqURncVPniRtGVAFzu4dM6KMwRr 5"));
    BOOST_CHECK_NO_THROW(r=CallRPC("getmsgsbyauthor keyID 1CevLPhmqURncVPniRtGVAFzu4dM6KMwRr"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 1);
    firstMessage = r.get_array().front().get_obj();
    BOOST_CHECK_EQUAL(find_value(firstMessage, "priority").get_int(), 50);
/*
    // TODO: fix
    BOOST_CHECK_NO_THROW(r=CallRPC("savemsgfromdata {\"signedData\":{\"timestamp\":1400788640,\"author\":[[\"keyID\",\"147cQZJ7Bd4ErnVYZahLfCaecJVkJVvqBP\"]],\"recipient\":[[\"keyID\",\"1Chdftd6Q9AbCih329udiYMNW46wpmS2nG\"]],\"type\":\"review\",\"comment\":\"test\",\"rating\":0,\"maxRating\":10,\"minRating\":-10},\"signature\":{\"pubKey\":\"PqxqDCkJ8h3Gj1ZqJp8qeWrrZGe5FuruJqj4YfBGj5BhUcSmphsuUWUc5eyoAwqP7N2WY7KAhpMtLuwCEiEbcEHV\",\"signature\":\"381yXZRZqUEtMPKFyzcTDJBsUgdMPvijTauLHK69jtZDVtAvGfKzeMfJjh5YdDKZcoLMHQQ7w3kL4JFipujc8EDE9kuJXoha\"}}"));
    BOOST_CHECK_NO_THROW(r=CallRPC("getmsgbyhash 1otjE8Sk6dFtax9V3f7jLceYxJXHJ7yFLd1btLuf82F"));
    firstMessage = r.get_array().front().get_obj();
    BOOST_CHECK_EQUAL(find_value(firstMessage, "priority").get_int(), 50);
*/

    BOOST_CHECK_NO_THROW(r=CallRPC("gettruststep p1 nobody1 p2 nobody2"));
    BOOST_CHECK(r.get_str().empty());
    BOOST_CHECK_NO_THROW(r=CallRPC("gettruststep keyID 1Jzbz2SsqnFpSrADASRywQEwZGZEY6y3As keyID 1CevLPhmqURncVPniRtGVAFzu4dM6KMwRr"));
    BOOST_CHECK(!r.get_str().empty());
    
    BOOST_CHECK_NO_THROW(r=CallRPC("getsavedpath p1 nobody1 p2 nobody2"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 0);
    BOOST_CHECK_NO_THROW(r=CallRPC("getsavedpath keyID 1Jzbz2SsqnFpSrADASRywQEwZGZEY6y3As keyID 1CevLPhmqURncVPniRtGVAFzu4dM6KMwRr"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 1);
    BOOST_CHECK_NO_THROW(r=CallRPC("getpath keyID 1Jzbz2SsqnFpSrADASRywQEwZGZEY6y3As email james@example.com"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 2);
    BOOST_CHECK_NO_THROW(r=CallRPC("getpath email james@example.com keyID 1Jzbz2SsqnFpSrADASRywQEwZGZEY6y3As"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 0);

    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email alice@example.com email bob@example.com 1"));
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email bob@example.com email carl@example.com 1"));
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email carl@example.com email david@example.com 1"));
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email david@example.com email bob@example.com 1"));

    BOOST_CHECK_NO_THROW(r=CallRPC("getpath p1 nobody1 p2 nobody2"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 0);
    BOOST_CHECK_NO_THROW(r=CallRPC("getpath email alice@example.com email bob@example.com"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 1);
    BOOST_CHECK_NO_THROW(r=CallRPC("getpath email alice@example.com email carl@example.com"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 2);
    BOOST_CHECK_NO_THROW(r=CallRPC("getpath email alice@example.com email david@example.com"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 3);
    BOOST_CHECK_NO_THROW(r=CallRPC("saveconnection email james@example.com email david@example.com url http://example.com/david"));
    BOOST_CHECK_NO_THROW(r=CallRPC("saveconnection email james@example.com url http://example.com/david account user@bitcoin-otc.com"));
    BOOST_CHECK_NO_THROW(r=CallRPC("getpath email alice@example.com account user@bitcoin-otc.com 5"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 5);
    BOOST_CHECK_NO_THROW(r=CallRPC("getpath email alice@example.com account user@bitcoin-otc.com 5"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 5);

    // Trust path should be one-way
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email david@example.com email emil@example.com 1"));
    BOOST_CHECK_NO_THROW(r=CallRPC("saverating email emil@example.com email fred@example.com 1"));
    BOOST_CHECK_NO_THROW(r=CallRPC("getpath email emil@example.com email david@example.com"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 0);
    BOOST_CHECK_NO_THROW(r=CallRPC("getpath email fred@example.com email david@example.com"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 0);
    BOOST_CHECK_NO_THROW(r=CallRPC("getpath email fred@example.com email bob@example.com"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 0);

    // Untrusted signer's msgs shouldn't create a trust path
    int i = CallRPC("getmsgcount").get_int();
    BOOST_CHECK_NO_THROW(r=CallRPC("savemsgfromdata {\"signature\":{\"pubKey\":\"MjK1zpuvL1RpRwe7AUJ7yTyayunzitqKGXkZm7JtgzagZDzQbZodcRE58cABTkFEXg8koXhbcefSaRkTzRxbHvCY\",\"signature\":\"AN1rKvtcLMRvLxNLY4DrV7zFeVVLfYczXZH4ZY7MJDcUB8yk2qf2MtLbvb3TuyTycXU57ThW7PusciNL14Q4myry1Nqub6Eio\"},\"signedData\":{\"author\":[[\"mbox\",\"mailto:alice@example.com\"]],\"comment\":\"thanks\",\"maxRating\":100,\"minRating\":-100,\"rating\":100,\"recipient\":[[\"mbox\",\"mailto:dick@example.com\"]],\"timestamp\":1,\"type\":\"review\"}}"));
    BOOST_CHECK_NO_THROW(r=CallRPC("getpath mbox mailto:alice@example.com mbox mailto:dick@example.com"));
    BOOST_CHECK_EQUAL(r.get_array().size(), 0);
    BOOST_CHECK_EQUAL(CallRPC("getmsgcount").get_int(), i + 1);
}

BOOST_AUTO_TEST_CASE(link_confs_and_refutes)
{
    resetDB();
    Value r;
    BOOST_CHECK_NO_THROW(r=CallRPC("savemsgfromdata {\"signedData\":{\"timestamp\":1234567,\"author\":[[\"email\",\"alice@example.com\"],[\"profile\",\"http://www.example.com/alice\"]],\"recipient\":[[\"email\",\"bob@example.com\"],[\"nickname\",\"BobTheBuilder\"]],\"type\":\"confirm_connection\",\"rating\":100,\"minRating\":-100,\"maxRating\":100},\"signature\":{}}"));
    BOOST_CHECK_NO_THROW(r=CallRPC("savemsgfromdata {\"signedData\":{\"timestamp\":1234568,\"author\":[[\"email\",\"alice@example.com\"],[\"profile\",\"http://www.example.com/alice\"]],\"recipient\":[[\"email\",\"bob@example.com\"],[\"nickname\",\"BobTheBuilder\"]],\"type\":\"confirm_connection\",\"rating\":100,\"minRating\":-100,\"maxRating\":100},\"signature\":{}}"));
    BOOST_CHECK_NO_THROW(r=CallRPC("savemsgfromdata {\"signedData\":{\"timestamp\":1234567,\"author\":[[\"email\",\"john@example.com\"]],\"recipient\":[[\"email\",\"bob@example.com\"],[\"nickname\",\"BobTheBuilder\"]],\"type\":\"confirm_connection\",\"rating\":100,\"minRating\":-100,\"maxRating\":100},\"signature\":{}}"));
    BOOST_CHECK_NO_THROW(r=CallRPC("savemsgfromdata {\"signedData\":{\"timestamp\":1234567,\"author\":[[\"email\",\"james@example.com\"]],\"recipient\":[[\"email\",\"bob@example.com\"],[\"nickname\",\"BobTheBuilder\"]],\"type\":\"refute_connection\",\"rating\":100,\"minRating\":-100,\"maxRating\":100},\"signature\":{}}"));
    BOOST_CHECK_NO_THROW(r=CallRPC("savemsgfromdata {\"signedData\":{\"timestamp\":1234567,\"author\":[[\"email\",\"james@example.com\"]],\"recipient\":[[\"email\",\"bob@example.com\"],[\"nickname\",\"BobTheBuilder\"]],\"type\":\"refute_connection\",\"rating\":100,\"minRating\":-100,\"maxRating\":100},\"signature\":{}}"));
    BOOST_CHECK_NO_THROW(r=CallRPC("getconnections email bob@example.com"));

    Array data = r.get_array();
    Object first = data.front().get_obj();
    BOOST_CHECK_EQUAL(find_value(first, "type").get_str(), "nickname");
    BOOST_CHECK_EQUAL(find_value(first, "value").get_str(), "BobTheBuilder");
    BOOST_CHECK_EQUAL(find_value(first, "confirmations").get_int(), 2);
    BOOST_CHECK_EQUAL(find_value(first, "refutations").get_int(), 1);

    BOOST_CHECK_NO_THROW(r=CallRPC("getconnectingmsgs email bob@example.com nickname BobTheBuilder"));
    BOOST_CHECK(!r.get_array().empty()); 

    BOOST_CHECK_NO_THROW(r=CallRPC("getconnectingmsgs email bob@example.com nickname BobTheBuilder 20 0 email alice@example.com 3 review"));
}

BOOST_AUTO_TEST_CASE(canonical_json)
{
    resetDB();
    Value r;
    // No whitespace
    BOOST_CHECK_THROW(r=CallRPC("savemsgfromdata {\"signedData\": {\"timestamp\":1234567,\"author\":[[\"email\",\"alice@example.com\"],[\"profile\",\"http://www.example.com/alice\"]],\"recipient\":[[\"email\",\"bob@example.com\"],[\"nickname\",\"BobTheBuilder\"]],\"type\":\"confirm_connection\",\"rating\":100,\"minRating\":-100,\"maxRating\":100},\"signature\":{}}"), runtime_error);
    // No line breaks
    BOOST_CHECK_THROW(r=CallRPC("savemsgfromdata {\"signedData\": \n{\"timestamp\":1234567,\"author\":[[\"email\",\"alice@example.com\"],[\"profile\",\"http://www.example.com/alice\"]],\"recipient\":[[\"email\",\"bob@example.com\"],[\"nickname\",\"BobTheBuilder\"]],\"type\":\"confirm_connection\",\"rating\":100,\"minRating\":-100,\"maxRating\":100},\"signature\":{}}"), runtime_error);
}

BOOST_AUTO_TEST_CASE(saverating_performance)
{
    resetDB();
    Value r;
    const char* rpcFormat = "saverating mbox mailto:alice@example.com mbox mailto:bob@example.com 1 %i";
    
    const int PACKET_COUNT = 5000;

    clock_t begin = clock();
    for (int i = 1; i <= PACKET_COUNT; i++) {
        char rpc[100];
        sprintf(rpc, rpcFormat, i);
        CallRPC(rpc);
        if (i % 200 == 0) {
            clock_t end = clock();
            double timeElapsed = double(end - begin) / CLOCKS_PER_SEC;
            double msgsPerSecond = i / timeElapsed;
            cout << i << " msgs saved in " << timeElapsed << " seconds, ";
            cout << msgsPerSecond << " msgs per second\n";
        }
    }
}

BOOST_AUTO_TEST_CASE(db_max_size)
{
    cout << "Testing DB size limit with 1 MB DB\n";
    delete pidentifidb;
    pidentifidb = new CIdentifiDB(1);
    Value r;
    const char* rpcFormat = "saverating mbox mailto:alice@example.com mbox mailto:bob@example.com 1 %i";
    for (int i = 0; i < 1600; i++) {
        char rpc[100];
        sprintf(rpc, rpcFormat, i);
        CallRPC(rpc);
        if (i % 100 == 0) cout << i << " msgs saved\n";
    }
    BOOST_CHECK_NO_THROW(r=CallRPC("getmsgcount"));
    int msgCount = r.get_int();
    for (int i = 0; i < 300; i++) {
        char rpc[100];
        sprintf(rpc, rpcFormat, i);
        CallRPC(rpc);
        if (i % 100 == 0) cout << i << " msgs saved\n";
    }
    BOOST_CHECK_NO_THROW(r=CallRPC("getmsgcount"));
    BOOST_CHECK(msgCount - 100 < r.get_int());
}

BOOST_AUTO_TEST_SUITE_END()
