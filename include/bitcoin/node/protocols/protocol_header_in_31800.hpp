/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_HEADER_IN_31800_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_PROTOCOL_HEADER_IN_31800_HPP

#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol.hpp>

namespace libbitcoin {
namespace node {
    
class BCN_API protocol_header_in_31800
  : public node::protocol,
    protected network::tracker<protocol_header_in_31800>
{
public:
    typedef std::shared_ptr<protocol_header_in_31800> ptr;

    template <typename SessionPtr>
    protocol_header_in_31800(const SessionPtr& session,
        const network::channel::ptr& channel) NOEXCEPT
      : node::protocol(session, channel),
        network::tracker<protocol_header_in_31800>(session->log)
    {
    }

    /// Start protocol (strand required).
    void start() NOEXCEPT override;

protected:
    virtual bool handle_receive_inventory(const code& ec,
        const network::messages::inventory::cptr& message) NOEXCEPT;
    virtual bool handle_receive_headers(const code& ec,
        const network::messages::headers::cptr& message) NOEXCEPT;
    virtual void handle_organize(const code& ec, size_t height,
        const system::chain::header::cptr& header_ptr) NOEXCEPT;
    virtual void complete() NOEXCEPT;

    // This is protected by strand.
    bool subscribed{};

private:
    network::messages::get_headers create_get_headers() const NOEXCEPT;
    network::messages::get_headers create_get_headers(
        const system::hash_digest& last) const NOEXCEPT;
    network::messages::get_headers create_get_headers(
        system::hashes&& start_hashes) const NOEXCEPT;
};

} // namespace node
} // namespace libbitcoin

#endif
