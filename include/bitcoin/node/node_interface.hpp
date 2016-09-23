/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-node.
 *
 * libbitcoin-node is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_NODE_P2P_NODE_INTERFACE_HPP
#define LIBBITCOIN_NODE_P2P_NODE_INTERFACE_HPP

#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/settings.hpp>

namespace libbitcoin {
namespace node {

/// A full node services interface.
class BCN_API node_interface
{
public:
    typedef blockchain::block_chain::reorganize_handler reorganize_handler;
    typedef blockchain::block_chain::transaction_handler transaction_handler;

    // Properties.
    // ------------------------------------------------------------------------

    /// Node configuration settings.
    virtual const node::settings& node_settings() const = 0;

    /// Return the current top block hash.
    virtual hash_digest top_hash() const = 0;

    /// Blockchain query interface.
    virtual blockchain::full_chain& chain() = 0;

    // Subscriptions.
    // ------------------------------------------------------------------------

    /// Subscribe to blockchain reorganization and stop events.
    virtual void subscribe_blockchain(reorganize_handler handler) = 0;

    /// Subscribe to transaction pool acceptance and stop events.
    virtual void subscribe_transaction(transaction_handler handler) = 0;
};

} // namespace node
} //namespace libbitcoin

#endif
