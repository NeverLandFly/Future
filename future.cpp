#include "future.hpp"
#include <eosiolib/contract.hpp>
#include <eosiolib/print.hpp>
#include <string.h>

class future: public contract{
public:
    future(account_name contract_account)
    :contract(contract_account),
     descriptions(_self, _self),
     order_book(_self, _self),
     clients(_self, _self){}

    static constexpr uint32_t max_num = (1L << 29) - 1;
    contract_description descriptions;
    orders order_book;
    client_index clients;

    //@abi action
    void info(account_name manager, uint32_t multiple, uint32_t leverage, std::string underlying, uint64_t expiration){
        require_auth(manager);
        eosio_assert(multiple<=100,"invalid multiple");
        eosio_assert(leverage<=100, "invalid leverage");
        auto itr = descriptions.find( _self );
        if (itr != descriptions.end()){
            descriptions.erase(itr);
        }
        auto info = descriptions.emplace(_self, [&](auto& info){
            info.manager = manager;
            info.contract_account = _self;
            info.multiple = multiple;
            info.leverage = leverage;
            info.underlying = underlying;
            info.expiration = expiration;
        });
    }

    //@abi action
    void insertorder(account_name owner, uint32_t price, uint32_t volume, uint8_t type) {
        require_auth(owner);
        eosio_assert(is_valid(price), "invalid price");
        eosio_assert(is_valid(price), "invalid volume");
        eosio_assert(type>=0 && type<=3, "invalid type");
        auto o = order{0, 0, price, volume, 0, type, owner};
        auto desc_itr = descriptions.find(_self);
        auto ask1key = desc_itr->ask1key;
        auto bid1key = desc_itr->bid1key;
        auto counter = desc_itr->counter;
        auto rcounter = desc_itr->rcounter;

        auto askitr = order_book.find(ask1key);
        auto idx = order_book.template get_index<N(rid)>();
        auto biditr = idx.find(bid1key);


        switch (type) {
            case 0: // buy open
            case 1: // buy close
                if (askitr != order_book.end()){
                    if (price < askitr->price){
                        auto rid = addordertobook(o, counter, rcounter,true);
                        if (biditr != idx.end() && price > biditr->price)
                            bid1key = rid;
                        else
                            bid1key = rid;
                        updateentrust(owner, volume, type);
                    }else{
                        auto current = volume;
                        while (current > 0 && ask1key != 0 && price >= askitr->price){
                            if (current < askitr->get_unclosed()){
                                updatedeal(askitr->owner, askitr->price, current, askitr->type,
                                           desc_itr->multiple);
                                updatedeal(owner, askitr->price, current, type,
                                           desc_itr->multiple);
                                order_book.modify(askitr, _self, [&](auto &o) {
                                    o.closedvolume += current;
                                current = 0;
                                });

                                break;
                            }else{
                                current -= askitr->get_unclosed();
                                updatedeal(askitr->owner, askitr->price, askitr->get_unclosed(), askitr->type,
                                           desc_itr->multiple);
                                updatedeal(owner, askitr->price, askitr->get_unclosed(), type,
                                           desc_itr->multiple);
                                if (askitr != order_book.end())
                                {
                                    auto tempitr = askitr++;
                                    order_book.erase(tempitr);
                                    ask1key = askitr->id;
                                } else{
                                    order_book.erase(askitr);
                                    ask1key = 0;
                                }

                            }
                        }
                        if (current > 0)
                        {
                            o.volume = volume;
                            o.closedvolume = volume - current;
                            auto rid = addordertobook(o, counter, rcounter, true);
                            bid1key = rid;
                            updateentrust(owner, volume, type);
                        }
                    }
                }else{
                    auto rid = addordertobook(o, counter, rcounter, true);
                    if (biditr != idx.end() && biditr->price < price){
                        bid1key = rid;
                    }else if (biditr == idx.end())
                        bid1key = rid;
                    updateentrust(owner, volume, type);
                }
                break;
            case 2: // sell open
            case 3: // sell close
                if (biditr != idx.end()){
                    if (price > biditr->price){
                        auto id = addordertobook(o, counter, rcounter);
                        if (askitr != order_book.end() && price < askitr->price)
                            ask1key = id;
                        else
                            ask1key = id;
                        updateentrust(owner, volume, type);
                    }else{
                        auto current = volume;
                        while (current > 0 && bid1key != 0 && price <= biditr->price){
                            if (current < biditr->get_unclosed()){
                                updatedeal(askitr->owner, askitr->price, current, askitr->type,
                                           desc_itr->multiple);
                                updatedeal(owner, askitr->price, current, type,
                                           desc_itr->multiple);
                                idx.modify(biditr, _self, [&](auto &o) {
                                    o.closedvolume += current;
                                });
                                current = 0;
                                break;
                            }else{
                                current -= biditr->get_unclosed();
                                updatedeal(askitr->owner, askitr->price, askitr->get_unclosed(), askitr->type,
                                           desc_itr->multiple);
                                updatedeal(owner, askitr->price, askitr->get_unclosed(), type,
                                           desc_itr->multiple);
                                if (biditr != idx.begin())
                                {
                                    auto tempitr = biditr--;
                                    idx.erase(tempitr);
                                    bid1key = biditr->rid;
                                }else{
                                    idx.erase(biditr);
                                    bid1key = 0;
                                }
                            }
                        }
                        if (current > 0)
                        {
                            o.volume = volume;
                            o.closedvolume = volume - current;
                            auto id = addordertobook(o, counter, rcounter);
                            ask1key = id;
                            updateentrust(owner, volume, type);
                        }
                    }
                }else{
                    auto id = addordertobook(o, counter, rcounter);
                    if (askitr != order_book.end() && askitr->price > price){
                        ask1key = id;
                    }else if (askitr == order_book.end())
                        ask1key = id;
                    updateentrust(owner, volume, type);
                }
                break;
        }
        descriptions.modify(desc_itr, _self, [&](auto &info) {
            info.counter++;
            info.rcounter--;
            info.bid1key = bid1key;
            info.ask1key = ask1key;
        });
    }

    void removeorder(uint64_t orderid){
        auto oitr = order_book.find( orderid );
        eosio_assert(oitr == order_book.end(), "order does not exist")
        require_auth(oitr->owner);
        auto citr = clients.find(oitr->owner);
        if (citr == clients.end()) {
            order_book.erase(oitr);
            return;
        }
        if (oitr->type == 0 || oitr->type == 1){
            clients.modify(citr, _self, [&](auto & client){
                client.longentrust -= oitr->volume;
            });
        }

        if (oitr->type == 2 || oitr->type == 3){
            clients.modify(citr, _self, [&](auto & client){
                client.shortentrust -= oitr->volume;
            });
        }
        order_book.erase(oitr);
    }

private:

    uint64_t addordertobook(order& o, uint32_t counter, uint32_t rcounter, bool retR = false){
        auto new_order = order_book.emplace(_self, [&](auto& new_order){
            new_order.id = ((uint64_t)o.price<<32) + counter;
            new_order.rid = ((uint64_t)o.price<<32) + rcounter;
            new_order.price = o.price;
            new_order.volume = o.volume;
            new_order.closedvolume = o.closedvolume;
            new_order.type = o.type;
            new_order.owner = o.owner;
        });
        return retR?new_order->rid:new_order->id;
    }

    void updateentrust(account_name account, uint32_t volume, uint8_t type){
        auto itr = clients.find(account);
        if (itr == clients.end()) return;
        if (type == 0 || type == 1){
            clients.modify(itr, _self, [&](auto & client){
                client.longentrust += volume;
            });
        }

        if (type == 2 || type == 3){
            clients.modify(itr, _self, [&](auto & client){
                client.shortentrust += volume;
            });
        }
    }

    void updatedeal(account_name account, uint32_t price, uint32_t volume, uint8_t type, uint32_t multiple){
        auto itr = clients.find(account);
        if (itr == clients.end()) return;
        if (type == 0) {
            clients.modify(itr, _self, [&](auto & client){
                client.longprice = (client.longprice * client.longpos + price * volume)/(client.longpos + volume);
                client.longpos += volume;
            });
        }

        if (type == 1) {
            clients.modify(itr, _self, [&](auto & client){
                client.balance.amount += (client.shortprice - price) * volume * multiple;
                client.shortprice = (client.shortprice * client.shortpos - price * volume)/(client.shortpos - volume);
                client.shortpos -= volume;
            });
        }

        if (type == 2) {
            clients.modify(itr, _self, [&](auto & client){
                client.shortprice = (client.shortprice * client.shortpos + price * volume)/(client.shortpos + volume);
                client.shortpos += volume;
            });
        }

        if (type == 3) {
            clients.modify(itr, _self, [&](auto & client){
                client.balance.amount += (price - client.longprice) * volume * multiple;
                client.longprice = (client.longprice * client.longpos - price * volume)/(client.longpos - volume);
                client.longpos -= volume;
            });
        }
    }

    bool is_valid(uint32_t value){
        return value < max_num;
    }
};

EOSIO_ABI(future, (info)(insertorder)(removeorder))