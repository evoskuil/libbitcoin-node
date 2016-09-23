/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_NODE_PROTOCOL_TRANSACTION_OUT_HPP
#define LIBBITCOIN_NODE_PROTOCOL_TRANSACTION_OUT_HPP

#include <atomic>
#include <cstdint>
#include <memory>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/node_interface.hpp>

namespace libbitcoin {
namespace node {

class BCN_API protocol_transaction_out
  : public network::protocol_events, track<protocol_transaction_out>
{
public:
    typedef std::shared_ptr<protocol_transaction_out> ptr;

    /// Construct a transaction protocol instance.
    protocol_transaction_out(node_interface& network,
        network::channel::ptr channel, blockchain::full_chain& blockchain);

    /// Start the protocol.
    virtual void start();

private:
    void send_transaction(const code& ec, transaction_ptr transaction,
        uint64_t height, const hash_digest& hash);

    bool handle_receive_get_data(const code& ec, get_data_const_ptr message);
    bool handle_receive_fee_filter(const code& ec,
        fee_filter_const_ptr message);
    void handle_receive_memory_pool(const code& ec,
        memory_pool_const_ptr message);

    void handle_stop(const code&);
    bool handle_floated(const code& ec,
        const chain::point::indexes& unconfirmed,
        transaction_const_ptr message);

    blockchain::full_chain& blockchain_;
    std::atomic<uint64_t> minimum_fee_;
    const bool relay_to_peer_;
};

} // namespace node
} // namespace libbitcoin

#endif
