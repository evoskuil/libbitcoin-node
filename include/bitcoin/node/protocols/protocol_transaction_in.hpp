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
#ifndef LIBBITCOIN_NODE_PROTOCOL_TRANSACTION_IN_HPP
#define LIBBITCOIN_NODE_PROTOCOL_TRANSACTION_IN_HPP

#include <memory>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/node_interface.hpp>

namespace libbitcoin {
namespace node {

class BCN_API protocol_transaction_in
  : public network::protocol_events, track<protocol_transaction_in>
{
public:
    typedef std::shared_ptr<protocol_transaction_in> ptr;

    /// Construct a transaction protocol instance.
    protocol_transaction_in(node_interface& network,
        network::channel::ptr channel, blockchain::full_chain& blockchain);

    /// Start the protocol.
    virtual void start();

private:
    void send_get_data(const code& ec, get_data_ptr message);

    void handle_filter_floaters(const code& ec, get_data_ptr message);
    bool handle_receive_inventory(const code& ec, inventory_const_ptr message);
    bool handle_receive_transaction(const code& ec,
        transaction_const_ptr message);
    void handle_store_transaction(const code& ec,
        const chain::point::indexes& unconfirmed,
        transaction_const_ptr message);
    bool handle_reorganized(const code& ec, size_t fork_height,
        const block_const_ptr_list& incoming,
        const block_const_ptr_list& outgoing);

    void handle_stop(const code&);

    blockchain::full_chain& blockchain_;
    const bool relay_from_peer_;
    const bool peer_suports_memory_pool_;
    const bool refresh_pool_;
};

} // namespace node
} // namespace libbitcoin

#endif
