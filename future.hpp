#include <eosiolib/eosio.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosiolib/asset.hpp>


using namespace eosio;
using namespace std;

//@abi table description i64
struct description {
    account_name contract_account;
    account_name manager;
    uint32_t multiple;
    uint32_t leverage;
    uint32_t counter = 0;
    uint32_t rcounter = (2L<<29) - 1;
    string underlying;
    uint64_t expiration;
    uint64_t ask1key = 0;
    uint64_t bid1key = 0;
    uint32_t lastprice = 0;

    auto primary_key() const { return contract_account; }
    EOSLIB_SERIALIZE(description, (contract_account)(manager)(multiple)(leverage)(counter)(rcounter)(underlying)(expiration)(ask1key)(bid1key)(lastprice))
};

typedef multi_index<N(description), description> contract_description;

//@abi table order i64
struct order{
    uint64_t id;
    uint64_t rid;
    uint32_t price;//trade price * eos precision(10000)
    uint32_t volume;
    uint32_t closedvolume;
    uint8_t type;//0=buy open, 1=buy sell, 2=sell open, 3=sell close
    account_name owner;

    auto primary_key() const { return id; }
    auto get_unclosed() const {return volume - closedvolume;}
    auto by_rid() const {return rid;}
    EOSLIB_SERIALIZE(order, (id)(rid)(price)(volume)(closedvolume)(type)(owner))
};
typedef multi_index<N(order), order,
    indexed_by<N(rid), const_mem_fun<order, uint64_t , &order::by_rid>>
> orders;

//@abi table user i64
struct client{
    account_name account;
    uint32_t longpos;
    uint32_t longprice;
    uint32_t longentrust;
    uint32_t shortpos;
    uint32_t shortprice;
    uint32_t shortentrust;
    asset balance;

    auto primary_key() const { return account; }
    EOSLIB_SERIALIZE(client, (account)(longpos)(longprice)(longentrust)(shortpos)(shortprice)(shortentrust)(balance))
};
typedef multi_index<N(client), client> client_index;