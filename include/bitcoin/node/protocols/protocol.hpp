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
#ifndef LIBBITCOIN_NODE_PROTOCOL_HPP
#define LIBBITCOIN_NODE_PROTOCOL_HPP

#include <cstddef>
#include <utility>
#include <bitcoin/network.hpp>
#include <bitcoin/node/node_interface.hpp>

namespace libbitcoin {
namespace node {

/// Intermediate protocol base class template.
/// This avoids having to make network::protocol into a template.
template <class Protocol>
class protocol
  : public Protocol
{
protected:
    /// Construct an instance.
    template <typename... Args>
    protocol(node_interface& network, network::channel::ptr channel,
        Args&&... args);

    /// Return the current top block height.
    virtual size_t top_height() const;

    /// Return the current top block hash.
    virtual hash_digest top_hash() const;

private:

    // This is thread safe.
    node_interface& node_network_;
};

} // namespace node
} // namespace libbitcoin

#include <bitcoin/node/impl/protocol.ipp>

#endif
